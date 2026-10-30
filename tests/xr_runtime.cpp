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
unsigned frame = 0, physical_frame = 0, event = 0, restart_phase = 0;
unsigned menu_completion_event = 0;
bool menu_gate() { return getenv("DOME_XR_FIXTURE_MENU_GATE") != nullptr; }
bool lost = false, regained = false, reference_event = false;
bool ink() { return getenv("DOME_XR_FIXTURE_STYLUS") != nullptr; }
std::string bake_mode(){auto mode=getenv("DOME_XR_FIXTURE_BAKE");return mode?mode:"";}
std::string skin_mode(){auto mode=getenv("DOME_XR_FIXTURE_SKIN");return mode?mode:"";}
std::string retopo_mode(){auto mode=getenv("DOME_XR_FIXTURE_RETOPO");return mode?mode:"";}
std::string cut_mode(){auto mode=getenv("DOME_XR_FIXTURE_CUT");return mode?mode:"";}
unsigned cut_step=0;
bool cut_preview_ready=false,cut_apply_ready=false;
std::string mask_mode(){auto mode=getenv("DOME_XR_FIXTURE_SCULPT_MASK");return mode?mode:"";}
std::string bake_policy_mode(){auto mode=getenv("DOME_XR_FIXTURE_BAKE_POLICY");return mode?mode:"";}
std::string procedural_mode(){auto mode=getenv("DOME_XR_FIXTURE_PROCEDURAL");return mode?mode:"";}
std::string resolution_mode(){auto mode=getenv("DOME_XR_FIXTURE_RESOLUTION");return mode?mode:"";}
std::string polygon_mode(){auto mode=getenv("DOME_XR_FIXTURE_POLYGON");return mode?mode:"";}
unsigned polygon_step=0;bool polygon_initial_ready=false,polygon_source_ready=false,polygon_selection_ready=false,polygon_edit_ready=false,polygon_motion_base_ready=false,polygon_motion_visible=false;
std::vector<uint8_t> polygon_motion_before;
unsigned mask_step=0;bool mask_work_ready=false, mask_work_completed=false, mask_released=false;
unsigned retopo_step = 0;
bool retopo_source_ready = false, retopo_apply_ready = false;

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
  if((!mask_mode().empty() && s->eye==0 && mask_step==35) || (!resolution_mode().empty() && s->eye==0 && frame==19)) {
    if(auto path=getenv("DOME_XR_FIXTURE_REPORT")) {
      std::ofstream capture(std::string(path)+(!resolution_mode().empty()?".resolution-"+std::to_string(frame):".mask-"+std::to_string(mask_step))+".ppm",std::ios::binary);
      capture<<"P6\n"<<s->width<<' '<<s->height<<"\n255\n";
      for(int y=int(s->height)-1;y>=0;--y)for(unsigned x=0;x<s->width;++x)for(int c=0;c<3;++c)
        capture.put(char(displayed(bytes[(size_t(y)*s->width+x)*4+c])));
    }
  }
  if(!polygon_mode().empty()&&(polygon_step==15||polygon_step==19||polygon_step==25||polygon_step==27)){
    const int sector=(polygon_step==25||polygon_step==27)?(polygon_mode()=="focus"?4:polygon_mode()=="noop"?10:7):0;
    const float a=sector*6.28318530718f/12;
    const float wx=.6f*220.f/640*std::cos(a),wy=1.56f+.6f/640*(220.f*std::sin(a)+16.f);
    const float eye=(s->eye?.032f:-.032f);
    const int cx=int(std::lround(s->width*.5f*(1+(wx-eye)/(.66f*std::tan(.6f)))));
    const int cy=int(std::lround(s->height*.5f*(1+(wy-1.6f)/(.66f*std::tan(.6f)))));
    int accent=0,total=0;
    for(int y=std::max(0,cy-1);y<=std::min(int(s->height)-1,cy+1);++y)for(int x=std::max(0,cx-8);x<=std::min(int(s->width)-1,cx+8);++x){
      size_t at=(size_t(y)*s->width+x)*4;++total;if(displayed(bytes[at])<150&&displayed(bytes[at+1])>130&&displayed(bytes[at+2])>220)++accent;
    }
    auto key=polygon_step==15?"polygon_initial_ready":polygon_step==19?"polygon_source_ready":polygon_step==25?"polygon_selection_ready":"polygon_edit_ready";
    report[std::string(key)+"_accent"]=std::max(report.value(std::string(key)+"_accent",0),accent);
    if(polygon_mode()=="focus"&&polygon_step==27){
      if(s->eye==0&&total&&accent==0){polygon_motion_before=bytes;polygon_motion_base_ready=true;report["polygon_motion_base_ready"]=true;}
    }else if(total&&accent*2>total){
      bool &ready=polygon_step==15?polygon_initial_ready:polygon_step==19?polygon_source_ready:polygon_step==25?polygon_selection_ready:polygon_edit_ready;
      if(!ready&&s->eye==0)if(auto path=getenv("DOME_XR_FIXTURE_REPORT")){
        std::ofstream capture(std::string(path)+"."+key+".ppm",std::ios::binary);capture<<"P6\n"<<s->width<<' '<<s->height<<"\n255\n";
        for(int y=int(s->height)-1;y>=0;--y)for(unsigned x=0;x<s->width;++x)for(int c=0;c<3;++c)capture.put(char(displayed(bytes[(size_t(y)*s->width+x)*4+c])));
      }
      ready=true;report[key]=true;
    }
  }
  if(polygon_mode()=="focus"&&polygon_step==29&&s->eye==0&&!polygon_motion_before.empty()){
    // Count only real surface coverage/shading, excluding grip/aim/ring and
    // selection colors so a changing cursor cannot acknowledge geometry.
    auto body=[&](const auto &image,size_t i){int r=displayed(image[i]),g=displayed(image[i+1]),b=displayed(image[i+2]);return image[i+3]>250&&b<220&&b>g+3&&g>r+3;};
    size_t changed=0;for(size_t i=0;i<bytes.size();i+=4){bool a=body(bytes,i),b=body(polygon_motion_before,i);
      if((a||b)&&(a||bytes[i+3]<5)&&(b||polygon_motion_before[i+3]<5)&&
        std::abs(int(bytes[i])-polygon_motion_before[i])+std::abs(int(bytes[i+1])-polygon_motion_before[i+1])+std::abs(int(bytes[i+2])-polygon_motion_before[i+2])>12)++changed;
    }
    report["polygon_motion_changed_pixels"]=std::max(report.value("polygon_motion_changed_pixels",size_t(0)),changed);
    if(changed>100){polygon_motion_visible=true;report["polygon_motion_visible"]=true;}
  }
  const bool cut_wait=!cut_mode().empty() && (cut_step==20 || (cut_mode()=="accept" && cut_step==30));
  const bool mask_gate=!mask_mode().empty() && (mask_step==68 || mask_step==79);
  if (cut_wait || mask_gate || (!retopo_mode().empty() && (retopo_step == 27 || retopo_step == 60))) {
    const bool initial=cut_wait?cut_step==20:retopo_step==27;
    const int sector = cut_wait?7:mask_gate || initial?0:10;
    float angle = sector * 6.28318530718f / 12;
    float world_x = (cut_wait?.045f:0.f) + .6f * 220.f / 640 * std::cos(angle);
    float world_y = 1.56f + .6f / 640 * (220.f * std::sin(angle) + (mask_gate ? 16.f : 0.f));
    float eye_x = (s->eye ? .032f : -.032f) + (frame >= 38 ? -.2f : 0);
    int cx = int(std::lround(s->width * .5f * (1 + (world_x - eye_x) / (((mask_gate||cut_wait)?.66f:1.01f) * std::tan(.6f)))));
    int cy = int(std::lround(s->height * .5f * (1 + (world_y - 1.6f) / (((mask_gate||cut_wait)?.66f:1.01f) * std::tan(.6f)))));
    int accent = 0, transparent=0, inspected = 0;
    const int half_height=mask_gate?1:3; // Sample the mask cell background above its text.
    for (int y = std::max(0,cy-half_height); y <= std::min(int(s->height)-1,cy+half_height); ++y)
      for (int x = std::max(0,cx-8); x <= std::min(int(s->width)-1,cx+8); ++x) {
        size_t at = (size_t(y) * s->width + x) * 4;
        ++inspected;
        transparent+=bytes[at+3]<5;
        if (displayed(bytes[at]) < 150 && displayed(bytes[at+1]) > 130 && displayed(bytes[at+2]) > 220) ++accent;
      }
    const char *pixels_key = cut_wait?(initial?"cut_preview_accent_pixels":"cut_apply_accent_pixels"):mask_gate ? "mask_work_accent_pixels" : retopo_step == 27 ? "retopo_source_accent_pixels" : "retopo_apply_accent_pixels";
    report[pixels_key] = std::max(report.value(pixels_key,0),accent);
    if(mask_gate && mask_step==68 && inspected && transparent*2>inspected) {
      mask_work_completed=true;report["mask_work_completed"]=true;
    }
    if ((!mask_gate || mask_step==79) && inspected && accent * 2 > inspected) {
      bool first = cut_wait?(initial?!cut_preview_ready:!cut_apply_ready):mask_gate ? !mask_work_ready : retopo_step == 27 ? !retopo_source_ready : !retopo_apply_ready;
      if (first) if (auto path = getenv("DOME_XR_FIXTURE_REPORT")) {
        std::ofstream capture(std::string(path) + (cut_wait?(initial?".cut-preview-ready.ppm":".cut-apply-ready.ppm"):mask_gate ? ".work-ready.ppm" : retopo_step == 27 ? ".source-ready.ppm" : ".apply-ready.ppm"), std::ios::binary);
        capture << "P6\n" << s->width << ' ' << s->height << "\n255\n";
        for (int y = int(s->height)-1; y >= 0; --y) for (unsigned x = 0; x < s->width; ++x)
          for (int channel = 0; channel < 3; ++channel)
            capture.put(char(displayed(bytes[(size_t(y)*s->width+x)*4+channel])));
      }
      if(cut_wait){
        if(initial){cut_preview_ready=true;report["cut_preview_ready"]=true;}
        else{cut_apply_ready=true;report["cut_apply_ready"]=true;}
      }else if (mask_gate) { mask_work_ready=true;report["mask_work_ready"]=true; }
      else if (retopo_step == 27) { retopo_source_ready = true; report["retopo_source_ready"] = true; }
      else { retopo_apply_ready = true; report["retopo_apply_ready"] = true; }
    }
  }
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
  if (menu_gate() && frame >= 45 && menu_completion_event < 2) {
    auto e = reinterpret_cast<XrEventDataSessionStateChanged *>(out);
    e->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
    e->session = reinterpret_cast<XrSession>(&session_tag);
    e->state = menu_completion_event++ == 0 ? XR_SESSION_STATE_STOPPING : XR_SESSION_STATE_EXITING;
    return XR_SUCCESS;
  }
  if(!polygon_mode().empty()&&polygon_step>=36&&menu_completion_event<2){
    auto e=reinterpret_cast<XrEventDataSessionStateChanged*>(out);e->type=XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
    e->session=reinterpret_cast<XrSession>(&session_tag);e->state=menu_completion_event++==0?XR_SESSION_STATE_STOPPING:XR_SESSION_STATE_EXITING;return XR_SUCCESS;
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
  auto retopo = retopo_mode();
  unsigned focus_frame = !polygon_mode().empty()?polygon_step:!cut_mode().empty()?cut_step:!mask_mode().empty() ? mask_step : retopo.empty() ? frame : retopo_step;
  unsigned focus_at = !cut_mode().empty()?90:!mask_mode().empty() ? (mask_mode()=="focus"?40:90) : retopo.empty() ? 30 : retopo == "focus" ? 40 : 90;
  if(!bake_policy_mode().empty())focus_at=bake_policy_mode()=="focus"?20:90;
  if(!procedural_mode().empty())focus_at=procedural_mode()=="focus"?20:90;
  if(!polygon_mode().empty())focus_at=polygon_mode()=="focus"?30:90;
  if(!resolution_mode().empty())focus_at=resolution_mode()=="focus"?20:90;
  if (focus_frame >= focus_at && !lost) {
    lost = true;
    auto e = reinterpret_cast<XrEventDataSessionStateChanged *>(out);
    e->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
    e->session = reinterpret_cast<XrSession>(&session_tag);
    e->state = XR_SESSION_STATE_VISIBLE;
    count("focus_losses");
    return XR_SUCCESS;
  }
  if (focus_frame >= focus_at + 3 && !regained) {
    regained = true;
    auto e = reinterpret_cast<XrEventDataSessionStateChanged *>(out);
    e->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
    e->session = reinterpret_cast<XrSession>(&session_tag);
    e->state = XR_SESSION_STATE_FOCUSED;
    count("focus_regains");
    return XR_SUCCESS;
  }
  if (polygon_mode().empty() && bake_policy_mode().empty() && procedural_mode().empty() && resolution_mode().empty() && retopo.empty() && mask_mode().empty() && cut_mode().empty() && frame >= 35 && !reference_event) {
    reference_event = true;
    auto e = reinterpret_cast<XrEventDataReferenceSpaceChangePending *>(out);
    e->type = XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING;
    e->session = reinterpret_cast<XrSession>(&session_tag);
    e->referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    e->changeTime = 1000000000 + (physical_frame + 3) * 18000000;
    e->poseValid = XR_TRUE;
    e->poseInPreviousSpace = {{0, 0, 0, 1}, {.2f, 0, 0}};
    count("reference_changes");
    return XR_SUCCESS;
  }
  return XR_EVENT_UNAVAILABLE;
}
FN(WaitFrame, XrSession, const XrFrameWaitInfo *, XrFrameState *p) {
  std::this_thread::sleep_for(std::chrono::milliseconds(18));
  ++physical_frame;
  // Keep delivering released input and real stereo frames until the menu has
  // actually reached a released swapchain. Advancing straight from click8 to
  // click10 can process open+close on the input worker between two snapshots.
  // This is a test-runtime acknowledgement, never an XR application wait.
  if (!(menu_gate() && frame == 9 && report.value("palette_images", 0) == 0)) ++frame;
  else count("menu_gate_wait_frames");
  if (menu_gate()) {
    report["menu_script_step"] = frame;
    if (physical_frame > 1200) {
      report["menu_gate_exhausted"] = true; save();
      return XR_ERROR_RUNTIME_FAILURE;
    }
  }
  if (auto mode=retopo_mode(); !mode.empty()) {
    bool waiting = (retopo_step == 27 && !retopo_source_ready) ||
                   (mode!="unconfirmed" && retopo_step == 60 && !retopo_apply_ready);
    if (!waiting) ++retopo_step;
    report["retopo_script_step"] = retopo_step;
  }
  if(auto mode=cut_mode();!mode.empty()){
    bool waiting=(cut_step==20&&!cut_preview_ready)||(mode=="accept"&&cut_step==30&&!cut_apply_ready);
    if(!waiting)++cut_step;
    report["cut_script_step"]=cut_step;
  }
  if(auto mode=mask_mode();!mode.empty()) {
    // A completed radial action disarms its input lease. The first released
    // sample rearms; a second released sample selects the next sector.
    if(mask_step==17 && !mask_released) mask_released=true;
    else if(!((mode=="fill" || mode=="undo") && ((mask_step==68 && !mask_work_completed) || (mask_step==79 && !mask_work_ready)))) ++mask_step;
    report["mask_script_step"]=mask_step;
  }
  if(auto mode=polygon_mode();!mode.empty()){
    const bool restore=mode.starts_with("restore");
    const bool waiting=(polygon_step==15&&!polygon_initial_ready)||(!restore&&polygon_step==19&&!polygon_source_ready)||(!restore&&polygon_step==25&&!polygon_selection_ready)||
      (mode=="focus"&&((polygon_step==27&&!polygon_motion_base_ready)||(polygon_step==29&&!polygon_motion_visible)))||
      ((mode=="accept"||mode=="cancel"||mode=="unconfirmed")&&polygon_step==27&&!polygon_edit_ready);
    if(!waiting)++polygon_step;else count("polygon_gate_wait_frames");
    report["polygon_script_step"]=polygon_step;
    if(physical_frame>1200){report["polygon_gate_exhausted"]=true;save();return XR_ERROR_RUNTIME_FAILURE;}
  }
  p->predictedDisplayTime = 1000000000 + physical_frame * 18000000;
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
                                        (frame >= 38 && polygon_mode().empty() ? -.2f : 0))) > .0001f)
        return XR_ERROR_POSE_INVALID;
    }
    count("stereo_submissions");
  } else
    count("empty_submissions");
  // Synchronize external interruption with a real in-progress controller
  // stroke; the parent never relies on an arbitrary startup delay.
  if(frame==16) if(auto marker=getenv("DOME_XR_FIXTURE_SIGNAL_READY"))
    std::ofstream(marker)<<frame<<'\n';
  if(menu_gate() && frame==9)save();
  if(!cut_mode().empty()||!polygon_mode().empty())save(); // Preserve actual progress even if a later stage stalls.
  return XR_SUCCESS;
}
FN(LocateViews, XrSession, const XrViewLocateInfo *, XrViewState *state,
   uint32_t cap, uint32_t *n, XrView *p) {
  *n = 2;
  state->viewStateFlags = frame == 27 && polygon_mode().empty() && bake_policy_mode().empty() && procedural_mode().empty() && resolution_mode().empty() && skin_mode()!="focus" && retopo_mode().empty() && mask_mode().empty() && cut_mode().empty() ? 0
                                      : XR_VIEW_STATE_POSITION_VALID_BIT |
                                            XR_VIEW_STATE_ORIENTATION_VALID_BIT;
  if (cap < 2)
    return XR_ERROR_SIZE_INSUFFICIENT;
  for (int i = 0; i < 2; ++i) {
    p[i].pose = {{0, 0, 0, 1},
                 {(i ? .032f : -.032f) + (frame >= 38 && polygon_mode().empty() ? -.2f : 0), 1.6f, 0}};
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
  if(auto mode=skin_mode();!mode.empty()) {
    out->currentState=button && (frame==8 || frame==10 || frame==12 || frame==14 || frame==16 || frame==18 || frame==20 ||
      (mode!="focus" && (frame==24 || frame==26)));
    out->changedSinceLastSync=button;
  }
  if (auto mode = retopo_mode(); !mode.empty()) {
    auto step = retopo_step;
    bool common = step==8 || step==10 || step==12 || step==16 || step==18 || step==20 ||
                  step==22 || step==24 || step==26 || step==28;
    bool apply = (mode=="accept" || mode=="focus") && (step==56 || step==58 || step==62);
    out->currentState = button && (common || apply);
    out->changedSinceLastSync = button;
  }
  if(auto mode=cut_mode();!mode.empty()){
    auto step=cut_step;
    out->currentState=button&&(step==8||step==10||step==12||step==14||step==16||step==18||step==22||
                               (mode=="accept"&&(step==26||step==32)));
    out->changedSinceLastSync=button;
  }
  if(auto mode=mask_mode();!mode.empty()) {
    auto step=mask_step;
    bool common=step==8 || step==10 || step==12 || step==14 || step==16 || step==18 || step==22 || step==24 || step==28 || step==30 || step==46 || step==48 || step==50 || step==52 || step==54;
    bool fill=(mode=="fill" || mode=="undo") && (step==66 || step==70 || step==72 || step==74 || step==76 || step==80);
    bool undo=mode=="undo" && (step==84 || step==86);
    out->currentState=button && (common || fill || undo);out->changedSinceLastSync=button;
    if(button)report["mask_buttons"][std::to_string(step)]=bool(out->currentState);
  }
  if(auto mode=bake_policy_mode();!mode.empty()) {
    out->currentState=button && (frame==8 || frame==10 || frame==12 || frame==14 || frame==16 || (mode!="focus" && frame==24));
    out->changedSinceLastSync=button;
  }
  if(!procedural_mode().empty()) {
    out->currentState=button && (frame==8 || frame==10 || frame==12 || frame==14 || frame==16);
    out->changedSinceLastSync=button;
  }
  if(!resolution_mode().empty()) {
    out->currentState=button && (frame==8 || frame==10 || frame==12 || frame==14);
    out->changedSinceLastSync=button;
  }
  if(auto mode=polygon_mode();!mode.empty()){
    auto step=polygon_step;const bool restore=mode.starts_with("restore");
    bool press=step==8||step==10||step==12||step==14||step==16;
    if(!restore)press|=step==18||step==20||step==24||step==26||((mode=="accept"||mode=="cancel")&&step==30);
    out->currentState=button&&press;out->changedSinceLastSync=button;
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
    if(!skin_mode().empty())stroke=frame==22;
    if (auto mode = retopo_mode(); !mode.empty()) {
      auto step = retopo_step;
      stroke = mode!="committed" && (step==32 || (mode=="focus" ? (step>=36 && step<=41) || step==48 || step==52
                                                                             : step==36 || step==40));
    }
    if(!cut_mode().empty())stroke=cut_step==24;
    if(auto mode=mask_mode();!mode.empty()) {
      auto step=mask_step;stroke=step==20 || step==26 || step==32 || step==56 || step==58 || step==62 || (mode=="focus" && step>=36 && step<=41);
    }
    if(auto mode=resolution_mode();!mode.empty())stroke=(mode!="noop" && frame==18) || ((mode=="accept" || mode=="cancel" || mode=="noop") && frame==22);
    if(auto mode=procedural_mode();!mode.empty())stroke=mode!="accept" && (frame==18 || (mode=="cancel" && frame==22));
    if(auto mode=bake_policy_mode();!mode.empty())stroke=frame==18 || (mode!="focus"&&frame==20);
    if(auto mode=polygon_mode();!mode.empty()){
      const bool restore=mode.starts_with("restore");
      stroke=restore?((mode!="restore-default"&&polygon_step==18)||polygon_step==22):
        mode=="noop"?false:polygon_step==22||(mode=="focus"&&polygon_step>=28&&polygon_step<=32);
    }
    bool correct =
        a->name == (ink() && frame >= 13 ? "tip_force" : "paint_pressure");
    if (stroke && correct) {
      out->currentState = .9f;
      if (!retopo_mode().empty()) count("retopo_pressure_samples");
      if(!polygon_mode().empty())count("polygon_pressure_samples");
      if (!cut_mode().empty()) count("cut_slider_samples");
      if (!mask_mode().empty()) count("mask_pressure_samples");
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
  if(!skin_mode().empty() && right(p->subactionPath) && a->name=="right_stick") {
    out->currentState={0,0};
    if(frame==9 || frame==10)out->currentState={-.5f,-.8660254f};
    if(frame==11 || frame==12 || frame==15 || frame==16)out->currentState={.5f,-.8660254f};
    if(frame==13 || frame==14)out->currentState={-.8660254f,.5f};
    if(frame==17 || frame==18)out->currentState={.8660254f,-.5f};
    if(frame==19 || frame==20)out->currentState={.8660254f,.5f};
    if(frame==25 || frame==26)out->currentState={-.8660254f,-.5f};
  }
  if (!retopo_mode().empty() && right(p->subactionPath) && a->name=="right_stick") {
    auto step=retopo_step;
    out->currentState={0,0};
    if(step==9 || step==10 || step==19 || step==20) out->currentState={0,-1};
    if(step==11 || step==12) out->currentState={.8660254f,.5f};
    if(step==17 || step==18) out->currentState={-.5f,-.8660254f};
    if(step==21 || step==22) out->currentState={.8660254f,-.5f};
    if(step==23 || step==24 || step==27 || step==28) out->currentState={1,0};
    if(step==57 || step==58) out->currentState={.5f,.8660254f};
    if(step>=60 && step<=62) out->currentState={.5f,-.8660254f};
  }
  if(!cut_mode().empty()&&right(p->subactionPath)&&a->name=="right_stick"){
    auto step=cut_step;out->currentState={0,0};
    if(step==9||step==10)out->currentState={-.5f,-.8660254f};
    if(step==11||step==12)out->currentState={0,-1};
    if(step==13||step==14)out->currentState={-.5f,.8660254f};
    if(step==15||step==16)out->currentState={1,0};
    if(step==19||step==20||(step>=29&&step<=32))out->currentState={-.8660254f,-.5f};
    if(step==21||step==22)out->currentState={.8660254f,.5f};
  }
  if(!mask_mode().empty() && right(p->subactionPath) && a->name=="right_stick") {
    auto step=mask_step;out->currentState={0,0};
    int sector=-1;
    if(step==9 || step==10 || step==47 || step==48) sector=8;
    if(step==11 || step==12 || step==13 || step==14 || step==49 || step==50 || step==51 || step==52) sector=11;
    if(step==15 || step==16) sector=6;
    if(step==17 || step==18) sector=2;
    if(step==23 || step==24) sector=1;
    if(step==29 || step==30 || (step>=68 && step<=69) || (step>=77 && step<=80)) sector=0;
    if(step==71 || step==72) sector=8;
    if(step==73 || step==74 || step==75 || step==76) sector=11;
    if(step==53 || step==54) sector=10;
    if(step==65 || step==66) sector=3;
    if(step==85 || step==86) sector=4;
    if(sector>=0) {float angle=sector*6.28318530718f/12;out->currentState={std::cos(angle),std::sin(angle)};}
    report["mask_vectors"][std::to_string(step)]={out->currentState.x,out->currentState.y};
  }
  if(!bake_policy_mode().empty() && right(p->subactionPath) && a->name=="right_stick") {
    out->currentState={0,0};int sector=-1;
    if(frame==9 || frame==10)sector=8;
    if(frame==11 || frame==12)sector=9;
    if(frame==13 || frame==14)sector=6;
    if(frame==15 || frame==16)sector=11;
    if(frame==23 || frame==24)sector=3;
    if(sector>=0){float angle=sector*6.28318530718f/12;out->currentState={std::cos(angle),std::sin(angle)};}
  }
  if(auto mode=procedural_mode();!mode.empty() && right(p->subactionPath) && a->name=="right_stick") {
    out->currentState={0,0};int sector=-1;
    if(frame==9 || frame==10)sector=8;
    if(frame==11 || frame==12)sector=11;
    if(frame==13 || frame==14)sector=9;
    if(frame==15 || frame==16)sector=mode=="accept"?6:4;
    if(sector>=0){float angle=sector*6.28318530718f/12;out->currentState={std::cos(angle),std::sin(angle)};}
  }
  if(!resolution_mode().empty() && right(p->subactionPath) && a->name=="right_stick") {
    out->currentState={0,0};int sector=-1;
    if(frame==9 || frame==10)sector=8;
    if(frame==11 || frame==12)sector=11;
    if(frame==13 || frame==14)sector=7;
    if(sector>=0){float angle=sector*6.28318530718f/12;out->currentState={std::cos(angle),std::sin(angle)};}
  }
  if(auto mode=polygon_mode();!mode.empty()&&right(p->subactionPath)&&a->name=="right_stick"){
    int sector=-1;auto step=polygon_step;
    if(step==9||step==10)sector=8;
    if(step==11||step==12)sector=9;
    if(step==13||step==14)sector=2;
    if(step==15||step==16||step==19||step==20)sector=0;
    if(step>=25&&step<=27)sector=mode=="focus"?4:mode=="noop"?10:7;
    if(step>=28&&step<=30)sector=mode=="cancel"?11:10;
    out->currentState={0,0};if(sector>=0){float a=sector*6.28318530718f/12;out->currentState={std::cos(a),std::sin(a)};}
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
  if (frame == 22 && polygon_mode().empty() && bake_policy_mode().empty() && procedural_mode().empty() && resolution_mode().empty() && s->action && voxel_mode() != "focus" && bake_mode().empty() && skin_mode().empty() && retopo_mode().empty() && mask_mode().empty() && cut_mode().empty()) {
    p->locationFlags = 0;
    count("tracking_losses");
  }
  p->pose.orientation = {0, 0, 0, 1};
  if (s->reference == XR_REFERENCE_SPACE_TYPE_VIEW && !s->action)
    p->pose.position = {frame >= 38 && polygon_mode().empty() ? -.2f : 0, 1.6f, 0};
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
  if(auto mode=skin_mode();!mode.empty() && s->action && s->action->name=="aim_pose" && right(s->hand) && frame==22) {
    float t=mode=="fitted"?0.f:1.f;
    p->pose.position={.045f+((100+420*t)/640-.5f)*.6f,1.56f+(.5f-300.f/640)*.6f,-.3f};
  }
  if(!cut_mode().empty()&&s->action&&s->action->name=="aim_pose"&&right(s->hand)&&cut_step==24){
    p->pose.position={.045f+((100+420*.6f)/640-.5f)*.6f,1.56f+(.5f-300.f/640)*.6f,-.3f};
  }
  if (auto mode=retopo_mode(); !mode.empty() && s->action && right(s->hand)) {
    float x=0,y=0;
    auto step=retopo_step;
    if(step==32) { x=-.2f; y=-.2f; }
    else if(mode=="focus" && step>=36 && step<=41) { x=.35f; y=.25f; }
    else if(step==36 || step==48) { x=.2f; y=-.2f; }
    else if(step==40 || step==52) { x=0; y=.2f; }
    p->pose.position={x,1.48f+y,-.83f};
  }
  if(auto mode=mask_mode();!mode.empty() && s->action && right(s->hand)) {
    auto step=mask_step;p->pose.position={0,1.48f,-.48f};
    if(step==32) p->pose.position={.11f,1.48f,-.67f};
    if(mode=="focus" && step>=36 && step<=41) p->pose.position={-.1f,1.60f,-.68f};
    if(s->action->name=="aim_pose" && (step==20 || step==26 || step==56 || step==58 || step==62)) {
      float x=100+420*.35f,y=300;
      if(step==26)x=100;
      if(step==56){x=100+420*.6f;y=210;}
      if(step==58){x=100;y=420;}
      if(step==62){x=mode=="accept"?470:170;y=609;}
      p->pose.position={(x/640-.5f)*.6f,1.56f+(.5f-y/640)*.6f,-.3f};
    }
  }
  if(auto mode=bake_policy_mode();!mode.empty() && s->action && right(s->hand)) {
    p->pose.position={0,1.48f,-.48f};
    if(s->action->name=="aim_pose" && (frame==18 || frame==20)) {
      float x=520,y=240;if(frame==20){x=mode=="cancel"?170:470;y=609;}
      p->pose.position={(x/640-.5f)*.6f,1.56f+(.5f-y/640)*.6f,-.3f};
    }
  }
  if(auto mode=procedural_mode();!mode.empty() && s->action && right(s->hand)) {
    p->pose.position={0,1.48f,-.48f};
    if(s->action->name=="aim_pose" && (frame==18 || frame==22)) {
      float x=100+420*.6f,y=300;
      if(frame==22){x=170;y=609;}
      p->pose.position={(x/640-.5f)*.6f,1.56f+(.5f-y/640)*.6f,-.3f};
    }
  }
  if(auto mode=resolution_mode();!mode.empty() && s->action && right(s->hand)) {
    p->pose.position={0,1.48f,-.48f};
    if(s->action->name=="aim_pose" && (frame==18 || frame==22)) {
      float x=100+420*float(std::log(.02/.00001)/std::log(100000.)),y=300;
      if(frame==22){x=mode=="cancel"?170:470;y=609;}
      p->pose.position={(x/640-.5f)*.6f,1.56f+(.5f-y/640)*.6f,-.3f};
    }
  }
  if(!polygon_mode().empty()&&s->action&&!right(s->hand))p->pose.position={-.3f,1.48f,-.48f};
  if(auto mode=polygon_mode();!mode.empty()&&s->action&&right(s->hand)){
    p->pose.position={0,1.48f,-.48f};
    if(s->action->name=="aim_pose"){
      p->pose.position={mode=="focus"&&polygon_step>=29?.1f:0.f,1.48f,-.1f};
      if(mode.starts_with("restore")&&(polygon_step==18||polygon_step==22)){
        float x=polygon_step==18?520:mode=="restore-cancel"?170:470,y=polygon_step==18?300:609;
        p->pose.position={(x/640-.5f)*.6f,1.56f+(.5f-y/640)*.6f,-.3f};
      }
    }
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
