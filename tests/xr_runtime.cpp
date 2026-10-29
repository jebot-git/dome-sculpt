// SPDX-License-Identifier: GPL-3.0-or-later
// A deterministic OpenXR test runtime. It creates real GL swapchain images;
// poses and actions are synthetic fixtures, never evidence of hardware
// tracking.
#define XR_NO_PROTOTYPES
#define XR_USE_PLATFORM_EGL
#define XR_USE_GRAPHICS_API_OPENGL
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>
#include <openxr/openxr_platform.h>
#include <thread>
#include <vector>
namespace {
using Json = nlohmann::json;
struct Action {
  std::string name;
  XrActionType type;
};
struct Space {
  Action *action = nullptr;
  XrPath hand = 0;
  XrReferenceSpaceType reference = XR_REFERENCE_SPACE_TYPE_LOCAL;
};
struct Swap {
  GLuint texture[2]{};
  GLuint read_fbo = 0;
  int64_t format = GL_RGBA8;
  uint32_t width, height, index = 0;
  bool acquired = false, waited = false;
  int eye;
};
std::vector<std::unique_ptr<Action>> actions;
std::vector<std::unique_ptr<Space>> spaces;
std::vector<std::unique_ptr<Swap>> chains;
std::map<std::string, XrPath> paths;
std::map<XrPath, std::string> names;
Json report = Json::object();
int instance_tag, session_tag, action_set_tag;
unsigned frame = 0, event = 0, restart_phase = 0;
bool lost = false, regained = false, reference_event = false;
bool ink() { return getenv("DOME_XR_FIXTURE_STYLUS") != nullptr; }
std::string bake_mode(){auto mode=getenv("DOME_XR_FIXTURE_BAKE");return mode?mode:"";}
std::string voxel_mode() {
  auto mode = getenv("DOME_XR_FIXTURE_VOXEL");
  return mode ? mode : "";
}
XrPath path(std::string text) {
  auto found = paths.find(text);
  if (found != paths.end())
    return found->second;
  XrPath value = paths.size() + 1;
  paths[text] = value;
  names[value] = text;
  return value;
}
bool right(XrPath p) { return names[p].find("right") != std::string::npos; }
void count(const char *key) { report[key] = report.value(key, 0) + 1; }
void save() {
  auto out = getenv("DOME_XR_FIXTURE_REPORT");
  if (out) {
    std::ofstream file(out);
    file << report.dump(2) << '\n';
  }
}
#define FN(name, ...) XrResult XRAPI_CALL f_##name(__VA_ARGS__)
FN(EnumerateInstanceExtensionProperties, const char *, uint32_t cap,
   uint32_t *n, XrExtensionProperties *out) {
  std::vector<const char *> values = {
      XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_MNDX_EGL_ENABLE_EXTENSION_NAME,
      "XR_LOGITECH_mx_ink_stylus_interaction", "XR_META_touch_controller_plus"};
  if (!getenv("DOME_XR_FIXTURE_NO_OVERLAY"))
    values.push_back(XR_EXTX_OVERLAY_EXTENSION_NAME);
  *n = values.size();
  if (!cap)
    return XR_SUCCESS;
  if (cap < *n)
    return XR_ERROR_SIZE_INSUFFICIENT;
  for (uint32_t i = 0; i < *n; ++i) {
    out[i].type = XR_TYPE_EXTENSION_PROPERTIES;
    strcpy(out[i].extensionName, values[i]);
    out[i].extensionVersion = 1;
  }
  return XR_SUCCESS;
}
FN(CreateInstance, const XrInstanceCreateInfo *info, XrInstance *out) {
  report = Json::object();
  report["application"] = info->applicationInfo.applicationName;
  *out = reinterpret_cast<XrInstance>(&instance_tag);
  return XR_SUCCESS;
}
FN(DestroyInstance, XrInstance) {
  save();
  actions.clear();
  spaces.clear();
  chains.clear();
  return XR_SUCCESS;
}
FN(GetInstanceProperties, XrInstance, XrInstanceProperties *p) {
  strcpy(p->runtimeName, "Dome Sculpt deterministic test runtime");
  p->runtimeVersion = XR_MAKE_VERSION(1, 0, 0);
  return XR_SUCCESS;
}
FN(GetSystem, XrInstance, const XrSystemGetInfo *, XrSystemId *id) {
  *id = 1;
  return XR_SUCCESS;
}
FN(GetSystemProperties, XrInstance, XrSystemId, XrSystemProperties *p) {
  p->systemId = 1;
  strcpy(p->systemName, "Synthetic stereo fixture");
  p->graphicsProperties = {1024, 1024, 8};
  p->trackingProperties = {XR_TRUE, XR_TRUE};
  return XR_SUCCESS;
}
FN(GetOpenGLGraphicsRequirementsKHR, XrInstance, XrSystemId,
   XrGraphicsRequirementsOpenGLKHR *p) {
  p->minApiVersionSupported = XR_MAKE_VERSION(3, 3, 0);
  p->maxApiVersionSupported = XR_MAKE_VERSION(4, 6, 0);
  return XR_SUCCESS;
}
FN(CreateSession, XrInstance, const XrSessionCreateInfo *info, XrSession *out) {
  auto binding = static_cast<const XrGraphicsBindingEGLMNDX *>(info->next);
  if (!binding || binding->type != XR_TYPE_GRAPHICS_BINDING_EGL_MNDX ||
      eglGetCurrentContext() != binding->context)
    return XR_ERROR_GRAPHICS_DEVICE_INVALID;
  report["egl_binding"] = true;
  report["overlay"] = binding->next != nullptr;
  *out = reinterpret_cast<XrSession>(&session_tag);
  return XR_SUCCESS;
}
FN(DestroySession, XrSession) {
  count("sessions_destroyed");
  save();
  return XR_SUCCESS;
}
FN(EnumerateReferenceSpaces, XrSession, uint32_t cap, uint32_t *n,
   XrReferenceSpaceType *p) {
  *n = 3;
  if (!cap)
    return XR_SUCCESS;
  if (cap < 3)
    return XR_ERROR_SIZE_INSUFFICIENT;
  p[0] = XR_REFERENCE_SPACE_TYPE_STAGE;
  p[1] = XR_REFERENCE_SPACE_TYPE_LOCAL;
  p[2] = XR_REFERENCE_SPACE_TYPE_VIEW;
  return XR_SUCCESS;
}
FN(CreateReferenceSpace, XrSession, const XrReferenceSpaceCreateInfo *p,
   XrSpace *out) {
  auto s = std::make_unique<Space>();
  s->reference = p->referenceSpaceType;
  *out = reinterpret_cast<XrSpace>(s.get());
  spaces.push_back(std::move(s));
  return XR_SUCCESS;
}
FN(DestroySpace, XrSpace p) {
  auto s = reinterpret_cast<Space *>(p);
  std::erase_if(spaces, [&](auto &v) { return v.get() == s; });
  return XR_SUCCESS;
}
FN(CreateActionSet, XrInstance, const XrActionSetCreateInfo *,
   XrActionSet *out) {
  *out = reinterpret_cast<XrActionSet>(&action_set_tag);
  return XR_SUCCESS;
}
FN(DestroyActionSet, XrActionSet) {
  actions.clear();
  return XR_SUCCESS;
}
FN(CreateAction, XrActionSet, const XrActionCreateInfo *p, XrAction *out) {
  auto a = std::make_unique<Action>(Action{p->actionName, p->actionType});
  *out = reinterpret_cast<XrAction>(a.get());
  actions.push_back(std::move(a));
  return XR_SUCCESS;
}
FN(DestroyAction, XrAction) { return XR_SUCCESS; }
FN(StringToPath, XrInstance, const char *value, XrPath *out) {
  *out = path(value);
  return XR_SUCCESS;
}
FN(PathToString, XrInstance, XrPath p, uint32_t cap, uint32_t *n, char *out) {
  if (!names.contains(p))
    return XR_ERROR_PATH_INVALID;
  *n = names[p].size() + 1;
  if (!cap)
    return XR_SUCCESS;
  if (cap < *n)
    return XR_ERROR_SIZE_INSUFFICIENT;
  memcpy(out, names[p].c_str(), *n);
  return XR_SUCCESS;
}
FN(SuggestInteractionProfileBindings, XrInstance,
   const XrInteractionProfileSuggestedBinding *p) {
  std::string profile = names[p->interactionProfile];
  for (uint32_t i = 0; i < p->countSuggestedBindings; ++i) {
    auto a = reinterpret_cast<Action *>(p->suggestedBindings[i].action);
    auto binding = names[p->suggestedBindings[i].binding];
    report["bindings"][profile][a->name].push_back(binding);
  }
  return XR_SUCCESS;
}
FN(AttachSessionActionSets, XrSession, const XrSessionActionSetsAttachInfo *) {
  return XR_SUCCESS;
}
FN(CreateActionSpace, XrSession, const XrActionSpaceCreateInfo *p,
   XrSpace *out) {
  auto s = std::make_unique<Space>();
  s->action = reinterpret_cast<Action *>(p->action);
  s->hand = p->subactionPath;
  *out = reinterpret_cast<XrSpace>(s.get());
  spaces.push_back(std::move(s));
  return XR_SUCCESS;
}
FN(EnumerateEnvironmentBlendModes, XrInstance, XrSystemId,
   XrViewConfigurationType, uint32_t cap, uint32_t *n,
   XrEnvironmentBlendMode *p) {
  *n = 1;
  if (cap)
    p[0] = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  return XR_SUCCESS;
}
FN(EnumerateViewConfigurationViews, XrInstance, XrSystemId,
   XrViewConfigurationType, uint32_t cap, uint32_t *n,
   XrViewConfigurationView *p) {
  *n = 2;
  if (!cap)
    return XR_SUCCESS;
  if (cap < 2)
    return XR_ERROR_SIZE_INSUFFICIENT;
  for (int i = 0; i < 2; ++i) {
    p[i].recommendedImageRectWidth = p[i].recommendedImageRectHeight = 320;
    p[i].maxImageRectWidth = p[i].maxImageRectHeight = 1024;
    p[i].recommendedSwapchainSampleCount = p[i].maxSwapchainSampleCount = 1;
  }
  return XR_SUCCESS;
}
FN(EnumerateSwapchainFormats, XrSession, uint32_t cap, uint32_t *n,
   int64_t *p) {
  *n = 1;
  if (cap)
    p[0] = getenv("DOME_XR_FIXTURE_SRGB") ? GL_SRGB8_ALPHA8 : GL_RGBA8;
  return XR_SUCCESS;
}
FN(CreateSwapchain, XrSession, const XrSwapchainCreateInfo *p,
   XrSwapchain *out) {
  auto s = std::make_unique<Swap>();
  s->width = p->width;
  s->height = p->height;
  s->eye = chains.size();
  s->format = p->format;
  report["swapchain_format"] = p->format;
  glGenFramebuffers(1, &s->read_fbo);
  glGenTextures(2, s->texture);
  for (auto id : s->texture) {
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, p->format, s->width, s->height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
  }
  *out = reinterpret_cast<XrSwapchain>(s.get());
  chains.push_back(std::move(s));
  count("swapchains_created");
  return XR_SUCCESS;
}
FN(DestroySwapchain, XrSwapchain p) {
  auto s = reinterpret_cast<Swap *>(p);
  glDeleteTextures(2, s->texture);
  glDeleteFramebuffers(1, &s->read_fbo);
  count("swapchains_destroyed");
  return XR_SUCCESS;
}
FN(EnumerateSwapchainImages, XrSwapchain p, uint32_t cap, uint32_t *n,
   XrSwapchainImageBaseHeader *out) {
  *n = 2;
  if (!cap)
    return XR_SUCCESS;
  if (cap < 2)
    return XR_ERROR_SIZE_INSUFFICIENT;
  auto s = reinterpret_cast<Swap *>(p);
  auto images = reinterpret_cast<XrSwapchainImageOpenGLKHR *>(out);
  for (int i = 0; i < 2; ++i)
    images[i].image = s->texture[i];
  return XR_SUCCESS;
}
FN(AcquireSwapchainImage, XrSwapchain p, const XrSwapchainImageAcquireInfo *,
   uint32_t *index) {
  auto s = reinterpret_cast<Swap *>(p);
  if (s->acquired)
    return XR_ERROR_CALL_ORDER_INVALID;
  s->acquired = true;
  s->waited = false;
  s->index = (s->index + 1) % 2;
  *index = s->index;
  count("acquires");
  return XR_SUCCESS;
}
FN(WaitSwapchainImage, XrSwapchain p, const XrSwapchainImageWaitInfo *) {
  auto s = reinterpret_cast<Swap *>(p);
  if (!s->acquired)
    return XR_ERROR_CALL_ORDER_INVALID;
  s->waited = true;
  count("image_waits");
  return XR_SUCCESS;
}
FN(ReleaseSwapchainImage, XrSwapchain p, const XrSwapchainImageReleaseInfo *) {
  auto s = reinterpret_cast<Swap *>(p);
  if (!s->acquired || !s->waited)
    return XR_ERROR_CALL_ORDER_INVALID;
  s->acquired = false;
  count("releases");
  std::vector<uint8_t> bytes(size_t(s->width) * s->height * 4);
  // Inspect the actual released image, independent of the application's read
  // framebuffer binding. RGBA8 swapchains contain linear premultiplied RGB;
  // SRGB8 swapchains store the corresponding sRGB transfer encoding.
  GLint previous_read = 0;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, s->read_fbo);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         s->texture[s->index], 0);
  if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    return XR_ERROR_RUNTIME_FAILURE;
  glReadPixels(0, 0, s->width, s->height, GL_RGBA, GL_UNSIGNED_BYTE,
               bytes.data());
  glBindFramebuffer(GL_READ_FRAMEBUFFER, GLuint(previous_read));
  if (glGetError() != GL_NO_ERROR) return XR_ERROR_RUNTIME_FAILURE;
  auto displayed = [&](uint8_t value) {
    if (s->format == GL_SRGB8_ALPHA8) return int(value);
    float linear = value / 255.f;
    float encoded = linear <= .0031308f ? linear * 12.92f
        : 1.055f * std::pow(linear, 1 / 2.4f) - .055f;
    return int(std::lround(encoded * 255));
  };
  int opaque = 0, blue = 0;
  for (size_t i = 0; i < bytes.size(); i += 4) {
    if (bytes[i + 3] > 0)
      ++opaque;
    if (displayed(bytes[i]) < 150 && displayed(bytes[i + 1]) > 130 && displayed(bytes[i + 2]) > 220)
      ++blue;
  }
  if (opaque > 100)
    count(s->eye ? "right_eye_nonblank" : "left_eye_nonblank");
  if (blue > 30)
    count("palette_images");
  return XR_SUCCESS;
}
FN(BeginSession, XrSession, const XrSessionBeginInfo *) {
  count("sessions_begun");
  return XR_SUCCESS;
}
FN(EndSession, XrSession) {
  count("sessions_ended");
  return XR_SUCCESS;
}
FN(RequestExitSession, XrSession) { return XR_SUCCESS; }
FN(PollEvent, XrInstance, XrEventDataBuffer *out) {
  if (event < 2) {
    auto e = reinterpret_cast<XrEventDataSessionStateChanged *>(out);
    e->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
    e->session = reinterpret_cast<XrSession>(&session_tag);
    e->state = event++ == 0 ? XR_SESSION_STATE_READY : XR_SESSION_STATE_FOCUSED;
    return XR_SUCCESS;
  }
  if (event == 2) {
    ++event;
    auto e = reinterpret_cast<XrEventDataInteractionProfileChanged *>(out);
    e->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
    e->session = reinterpret_cast<XrSession>(&session_tag);
    return XR_SUCCESS;
  }
  if (getenv("DOME_XR_FIXTURE_RESTART") && frame >= 23 && restart_phase < 8) {
    static constexpr XrSessionState states[] = {
        XR_SESSION_STATE_VISIBLE,  XR_SESSION_STATE_SYNCHRONIZED,
        XR_SESSION_STATE_STOPPING, XR_SESSION_STATE_IDLE,
        XR_SESSION_STATE_READY,    XR_SESSION_STATE_SYNCHRONIZED,
        XR_SESSION_STATE_VISIBLE,  XR_SESSION_STATE_FOCUSED};
    if (restart_phase == 3 && report.value("sessions_ended", 0) != 1)
      return XR_ERROR_RUNTIME_FAILURE;
    auto e = reinterpret_cast<XrEventDataSessionStateChanged *>(out);
    e->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
    e->session = reinterpret_cast<XrSession>(&session_tag);
    e->state = states[restart_phase++];
    count("restart_states");
    return XR_SUCCESS;
  }
  if (frame >= 30 && !lost) {
    lost = true;
    auto e = reinterpret_cast<XrEventDataSessionStateChanged *>(out);
    e->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
    e->session = reinterpret_cast<XrSession>(&session_tag);
    e->state = XR_SESSION_STATE_VISIBLE;
    count("focus_losses");
    return XR_SUCCESS;
  }
  if (frame >= 33 && !regained) {
    regained = true;
    auto e = reinterpret_cast<XrEventDataSessionStateChanged *>(out);
    e->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
    e->session = reinterpret_cast<XrSession>(&session_tag);
    e->state = XR_SESSION_STATE_FOCUSED;
    count("focus_regains");
    return XR_SUCCESS;
  }
  if (frame >= 35 && !reference_event) {
    reference_event = true;
    auto e = reinterpret_cast<XrEventDataReferenceSpaceChangePending *>(out);
    e->type = XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING;
    e->session = reinterpret_cast<XrSession>(&session_tag);
    e->referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    e->changeTime = 1000000000 + 38 * 18000000;
    e->poseValid = XR_TRUE;
    e->poseInPreviousSpace = {{0, 0, 0, 1}, {.2f, 0, 0}};
    count("reference_changes");
    return XR_SUCCESS;
  }
  return XR_EVENT_UNAVAILABLE;
}
FN(WaitFrame, XrSession, const XrFrameWaitInfo *, XrFrameState *p) {
  std::this_thread::sleep_for(std::chrono::milliseconds(18));
  ++frame;
  p->predictedDisplayTime = 1000000000 + frame * 18000000;
  p->predictedDisplayPeriod = 18000000;
  p->shouldRender = frame % 7 ? XR_TRUE : XR_FALSE;
  count("frames_waited");
  return XR_SUCCESS;
}
FN(BeginFrame, XrSession, const XrFrameBeginInfo *) {
  count("frames_begun");
  return XR_SUCCESS;
}
FN(EndFrame, XrSession, const XrFrameEndInfo *p) {
  count("frames_ended");
  if (p->layerCount) {
    if (p->layerCount != 1 ||
        p->layers[0]->type != XR_TYPE_COMPOSITION_LAYER_PROJECTION)
      return XR_ERROR_LAYER_INVALID;
    auto layer =
        reinterpret_cast<const XrCompositionLayerProjection *>(p->layers[0]);
    if (layer->viewCount != 2)
      return XR_ERROR_LAYER_INVALID;
    for (int eye = 0; eye < 2; ++eye) {
      auto &v = layer->views[eye];
      if (v.subImage.imageRect.extent.width != 320 ||
          v.subImage.imageRect.extent.height != 320 ||
          std::abs(v.pose.position.x - ((eye ? .032f : -.032f) +
                                        (frame >= 38 ? -.2f : 0))) > .0001f)
        return XR_ERROR_POSE_INVALID;
    }
    count("stereo_submissions");
  } else
    count("empty_submissions");
  return XR_SUCCESS;
}
FN(LocateViews, XrSession, const XrViewLocateInfo *, XrViewState *state,
   uint32_t cap, uint32_t *n, XrView *p) {
  *n = 2;
  state->viewStateFlags = frame == 27 ? 0
                                      : XR_VIEW_STATE_POSITION_VALID_BIT |
                                            XR_VIEW_STATE_ORIENTATION_VALID_BIT;
  if (cap < 2)
    return XR_ERROR_SIZE_INSUFFICIENT;
  for (int i = 0; i < 2; ++i) {
    p[i].pose = {{0, 0, 0, 1},
                 {(i ? .032f : -.032f) + (frame >= 38 ? -.2f : 0), 1.6f, 0}};
    p[i].fov = {-.6f, .6f, .6f, -.6f};
  }
  return XR_SUCCESS;
}
FN(SyncActions, XrSession, const XrActionsSyncInfo *) {
  count("action_syncs");
  return XR_SUCCESS;
}
FN(GetActionStateBoolean, XrSession, const XrActionStateGetInfo *p,
   XrActionStateBoolean *out) {
  auto a = reinterpret_cast<Action *>(p->action);
  bool button = right(p->subactionPath) && a->name == "recenter";
  out->isActive = XR_TRUE;
  out->currentState =
      button &&
      (frame == 8 || frame == 10 ||
       (getenv("DOME_XR_FIXTURE_PAINT") &&
        (frame == 12 || frame == 14 || frame == 16 || frame == 18)) ||
       (getenv("DOME_XR_FIXTURE_CONTROLS") && (frame == 12 || frame == 21)) ||
       (getenv("DOME_XR_FIXTURE_UV") && frame == 12) ||
       (getenv("DOME_XR_FIXTURE_LAYERS") && (frame == 12 || frame == 14)) ||
       (getenv("DOME_XR_FIXTURE_SCENE") &&
        (frame == 12 || frame == 16 || frame == 18 || frame == 20)) ||
       (getenv("DOME_XR_FIXTURE_FILES") &&
        (frame == 24 || frame == 26 || frame == 28 || frame == 30)));
  out->changedSinceLastSync = button && (frame >= 8 && frame <= 11);
  if (auto mode = voxel_mode(); !mode.empty()) {
    out->currentState = button &&
        (frame == 8 || frame == 10 || frame == 12 || frame == 14 ||
         ((mode == "accept" || mode == "undo") && frame == 18) ||
         (mode == "undo" && (frame == 42 || frame == 44)));
    out->changedSinceLastSync = button;
  }
  if(auto mode=bake_mode();!mode.empty()){
    out->currentState=button && (frame==8||frame==10||frame==12||frame==14||frame==16||((mode=="accept"||mode=="undo")&&(frame==20||frame==24))||(mode=="undo"&&(frame==42||frame==44)));
    out->changedSinceLastSync=button;
  }
  return XR_SUCCESS;
}
FN(GetActionStateFloat, XrSession, const XrActionStateGetInfo *p,
   XrActionStateFloat *out) {
  auto a = reinterpret_cast<Action *>(p->action);
  bool active = a->name == "paint_pressure" || a->name == "canvas_grab" ||
                (a->name == "tip_force" && ink());
  out->isActive = active;
  out->currentState = 0;
  if (right(p->subactionPath)) {
    bool stroke =
        (frame >= 2 && frame <= 5) ||
        (!getenv("DOME_XR_FIXTURE_SCENE") && frame >= 13 && frame <= 18);
    if (getenv("DOME_XR_FIXTURE_PAINT"))
      stroke = (frame >= 2 && frame <= 5) || (frame == 20);
    if (getenv("DOME_XR_FIXTURE_CONTROLS"))
      stroke = frame == 19;
    if (getenv("DOME_XR_FIXTURE_UV"))
      stroke = frame == 14 || frame == 20;
    if (getenv("DOME_XR_FIXTURE_LAYERS"))
      stroke = frame >= 18 && frame <= 20;
    if (getenv("DOME_XR_FIXTURE_RESTART"))
      stroke = (frame >= 2 && frame <= 5) || (frame >= 13 && frame <= 18) ||
               frame == 25;
    if (auto mode = voxel_mode(); !mode.empty())
      stroke = frame == 16 || (mode == "cancel" && frame == 18);
    if(auto mode=bake_mode();!mode.empty())stroke=frame==18||(mode=="cancel"&&frame==20);
    bool correct =
        a->name == (ink() && frame >= 13 ? "tip_force" : "paint_pressure");
    if (stroke && correct) {
      out->currentState = .9f;
      count(a->name == "tip_force" ? "tip_pressure_samples"
                                   : "trigger_pressure_samples");
    }
  }
  return XR_SUCCESS;
}
FN(GetActionStateVector2f, XrSession, const XrActionStateGetInfo *p,
   XrActionStateVector2f *out) {
  auto a = reinterpret_cast<Action *>(p->action);
  out->isActive = XR_TRUE;
  out->currentState = right(p->subactionPath) && a->name == "right_stick" &&
                              frame >= 9 && frame <= 10
                          ? XrVector2f{.8660254f, .5f}
                          : XrVector2f{0, 0};
  if (getenv("DOME_XR_FIXTURE_FILES") && right(p->subactionPath) &&
      a->name == "right_stick") {
    if (frame == 25 || frame == 26)
      out->currentState = {-1, 0};
    if (frame == 29 || frame == 30)
      out->currentState = {-.8660254f, -.5f};
  }
  if (getenv("DOME_XR_FIXTURE_SCENE") && right(p->subactionPath) &&
      a->name == "right_stick") {
    if (frame == 9 || frame == 10)
      out->currentState = {-.5f, -.8660254f};
    if (frame == 11 || frame == 12)
      out->currentState = {1, 0};
    if (frame == 17 || frame == 18)
      out->currentState = {0, -1};
    if (frame == 19 || frame == 20)
      out->currentState = {.8660254f, .5f};
  }
  if (getenv("DOME_XR_FIXTURE_PAINT") && right(p->subactionPath) &&
      a->name == "right_stick") {
    if (frame == 9 || frame == 10)
      out->currentState = {0, -1};
    if (frame == 11 || frame == 12)
      out->currentState = {0, 1};
    if (frame == 13 || frame == 14)
      out->currentState = {.8660254f, -.5f};
    if (frame == 15 || frame == 16)
      out->currentState = {0, -1};
    if (frame == 17 || frame == 18)
      out->currentState = {.5f, .8660254f};
  }
  if (getenv("DOME_XR_FIXTURE_CONTROLS") && right(p->subactionPath) &&
      a->name == "right_stick") {
    if (frame == 9 || frame == 10)
      out->currentState = {0, -1};
    if (frame == 11 || frame == 12)
      out->currentState = {-.5f, .8660254f};
    if (frame >= 14 && frame <= 18)
      out->currentState = {1, 0};
  }
  if (getenv("DOME_XR_FIXTURE_UV") && right(p->subactionPath) &&
      a->name == "right_stick") {
    if (frame == 9 || frame == 10)
      out->currentState = {0, -1};
    if (frame == 11 || frame == 12)
      out->currentState = {-.8660254f, -.5f};
    if (frame >= 15 && frame <= 18)
      out->currentState = {-1, 0};
  }
  if (getenv("DOME_XR_FIXTURE_LAYERS") && right(p->subactionPath) &&
      a->name == "right_stick") {
    if (frame == 9 || frame == 10)
      out->currentState = {-.5f, -.8660254f};
    if (frame == 11 || frame == 12)
      out->currentState = {.8660254f, -.5f};
    if (frame == 13 || frame == 14)
      out->currentState = {1, 0};
  }
  if (auto mode = voxel_mode(); !mode.empty() && right(p->subactionPath) &&
      a->name == "right_stick") {
    out->currentState = {0, 0};
    if (frame == 9 || frame == 10) out->currentState = {-.5f, -.8660254f};
    if (frame == 11 || frame == 12) out->currentState = {0, -1};
    if (frame == 13 || frame == 14) out->currentState = {.8660254f, .5f};
    if (mode == "undo" && (frame == 43 || frame == 44))
      out->currentState = {-.5f, .8660254f};
  }
  if(auto mode=bake_mode();!mode.empty() && right(p->subactionPath) && a->name=="right_stick"){
    out->currentState={0,0};
    if(frame==9||frame==10)out->currentState={-.5f,-.8660254f};
    if(frame==11||frame==12)out->currentState={0,-1};
    if(frame==13||frame==14)out->currentState={-1,0};
    if(frame==15||frame==16)out->currentState={.8660254f,.5f};
    if((mode=="accept"||mode=="undo") && frame>=21 && frame<=24)out->currentState={0,1};
    if(mode=="undo"&&(frame==43||frame==44))out->currentState={-.5f,.8660254f};
  }
  return XR_SUCCESS;
}
FN(GetActionStatePose, XrSession, const XrActionStateGetInfo *,
   XrActionStatePose *p) {
  p->isActive = XR_TRUE;
  return XR_SUCCESS;
}
FN(GetCurrentInteractionProfile, XrSession, XrPath hand,
   XrInteractionProfileState *p) {
  p->interactionProfile =
      path(right(hand) && ink()
               ? "/interaction_profiles/logitech/mx_ink_stylus_logitech"
               : "/interaction_profiles/meta/touch_controller_plus");
  return XR_SUCCESS;
}
FN(LocateSpace, XrSpace handle, XrSpace, XrTime, XrSpaceLocation *p) {
  auto s = reinterpret_cast<Space *>(handle);
  p->locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT |
                     XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  if (frame == 22 && s->action && voxel_mode() != "focus" && bake_mode().empty()) {
    p->locationFlags = 0;
    count("tracking_losses");
  }
  p->pose.orientation = {0, 0, 0, 1};
  if (s->reference == XR_REFERENCE_SPACE_TYPE_VIEW && !s->action)
    p->pose.position = {frame >= 38 ? -.2f : 0, 1.6f, 0};
  else
    p->pose.position = {(right(s->hand) ? float((frame % 5) * .015f) : -.3f) +
                            (frame >= 38 ? -.2f : 0),
                        1.48f, -.48f};
  if (getenv("DOME_XR_FIXTURE_RESTART") && s->action && right(s->hand) &&
      frame >= 24 && frame <= 26)
    p->pose.position = {-.22f, 1.48f, -.48f};
  if (getenv("DOME_XR_FIXTURE_CONTROLS") && s->action &&
      s->action->name == "aim_pose" && right(s->hand) && frame == 19) {
    p->pose.position = {.045f + (((100 + 420 * .75f) / 640) - .5f) * .6f,
                        1.56f + (.5f - 300.f / 640) * .6f, -.3f};
  }
  if (getenv("DOME_XR_FIXTURE_UV") && s->action &&
      s->action->name == "aim_pose" && right(s->hand) &&
      (frame == 14 || frame == 20)) {
    float x = frame == 14 ? 178 : 470, y = frame == 14 ? 85 : 609;
    p->pose.position = {.045f + (x / 640 - .5f) * .6f,
                        1.56f + (.5f - y / 640) * .6f, -.3f};
  }
  if (auto mode = voxel_mode(); !mode.empty() && s->action &&
      s->action->name == "aim_pose" && right(s->hand) &&
      (frame == 16 || (mode == "cancel" && frame == 18))) {
    float t = std::log(50.f / .01f) / std::log(1000.f / .01f);
    float x = frame == 16 ? 100 + 420 * t : 170;
    float y = frame == 16 ? 300 : 609;
    p->pose.position = {.045f + (x / 640 - .5f) * .6f,
                        1.56f + (.5f - y / 640) * .6f, -.3f};
  }
  if(auto mode=bake_mode();!mode.empty() && s->action && s->action->name=="aim_pose" && right(s->hand) && (frame==18||(mode=="cancel"&&frame==20))){
    float t=std::log(20.f/.01f)/std::log(10000.f/.01f);
    float x=frame==18?100+420*t:170,y=frame==18?240:609;
    p->pose.position={.045f+(x/640-.5f)*.6f,1.56f+(.5f-y/640)*.6f,-.3f};
  }
  return XR_SUCCESS;
}
FN(ApplyHapticFeedback, XrSession, const XrHapticActionInfo *,
   const XrHapticBaseHeader *) {
  count("haptics");
  return XR_SUCCESS;
}
FN(StopHapticFeedback, XrSession, const XrHapticActionInfo *) {
  return XR_SUCCESS;
}
FN(GetInstanceProcAddr, XrInstance, const char *name, PFN_xrVoidFunction *out) {
#define ENTRY(s)                                                               \
  if (!strcmp(name, "xr" #s)) {                                                \
    *out = reinterpret_cast<PFN_xrVoidFunction>(f_##s);                        \
    return XR_SUCCESS;                                                         \
  }
  ENTRY(GetInstanceProcAddr)
  ENTRY(EnumerateInstanceExtensionProperties)
  ENTRY(CreateInstance)
  ENTRY(DestroyInstance)
  ENTRY(GetInstanceProperties)
  ENTRY(GetSystem)
  ENTRY(GetSystemProperties)
  ENTRY(GetOpenGLGraphicsRequirementsKHR)
  ENTRY(CreateSession)
  ENTRY(DestroySession)
  ENTRY(EnumerateReferenceSpaces)
  ENTRY(CreateReferenceSpace)
  ENTRY(DestroySpace)
  ENTRY(CreateActionSet)
  ENTRY(DestroyActionSet)
  ENTRY(CreateAction)
  ENTRY(DestroyAction) ENTRY(StringToPath) ENTRY(PathToString)
      ENTRY(SuggestInteractionProfileBindings) ENTRY(AttachSessionActionSets)
          ENTRY(CreateActionSpace) ENTRY(EnumerateEnvironmentBlendModes) ENTRY(
              EnumerateViewConfigurationViews) ENTRY(EnumerateSwapchainFormats)
              ENTRY(CreateSwapchain) ENTRY(DestroySwapchain) ENTRY(
                  EnumerateSwapchainImages) ENTRY(AcquireSwapchainImage)
                  ENTRY(WaitSwapchainImage) ENTRY(ReleaseSwapchainImage) ENTRY(
                      BeginSession) ENTRY(EndSession) ENTRY(RequestExitSession)
                      ENTRY(PollEvent) ENTRY(WaitFrame) ENTRY(BeginFrame)
                          ENTRY(EndFrame) ENTRY(LocateViews) ENTRY(SyncActions)
                              ENTRY(GetActionStateBoolean)
                                  ENTRY(GetActionStateFloat)
                                      ENTRY(GetActionStateVector2f)
                                          ENTRY(GetActionStatePose) ENTRY(
                                              GetCurrentInteractionProfile)
                                              ENTRY(LocateSpace)
                                                  ENTRY(ApplyHapticFeedback)
                                                      ENTRY(StopHapticFeedback)
#undef ENTRY
                                                          *out = nullptr;
  return XR_ERROR_FUNCTION_UNSUPPORTED;
}
} // namespace
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo *loader, XrNegotiateRuntimeRequest *request) {
  if (!loader || !request || loader->maxInterfaceVersion < 1)
    return XR_ERROR_INITIALIZATION_FAILED;
  request->runtimeInterfaceVersion = 1;
  request->runtimeApiVersion = XR_MAKE_VERSION(1, 0, 0);
  request->getInstanceProcAddr = f_GetInstanceProcAddr;
  return XR_SUCCESS;
}
