// SPDX-License-Identifier: GPL-3.0-or-later
// Session lifecycle adapted from OpenBrush Native (Apache-2.0); see
// third_party/openbrush/NOTICE.
#include "dome/xr.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>
namespace dome {
using ob::controllerBindings;
using ob::controllerProfiles;
namespace {
void check(XrResult result, const char *call) {
  if (XR_FAILED(result))
    throw std::runtime_error(std::string(call) + ": OpenXR error " +
                             std::to_string(result));
}
#define XR(call) check(call, #call)
std::vector<XrExtensionProperties> extensions() {
  uint32_t n = 0;
  XR(xrEnumerateInstanceExtensionProperties(nullptr, 0, &n, nullptr));
  std::vector<XrExtensionProperties> p(n, {XR_TYPE_EXTENSION_PROPERTIES});
  XR(xrEnumerateInstanceExtensionProperties(nullptr, n, &n, p.data()));
  return p;
}
bool has(const std::vector<XrExtensionProperties> &list, const char *name) {
  return std::any_of(list.begin(), list.end(),
                     [&](auto &p) { return !strcmp(p.extensionName, name); });
}
XrInstance create(const std::vector<const char *> &ext) {
  XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
  strcpy(info.applicationInfo.applicationName, "Dome Sculpt");
  strcpy(info.applicationInfo.engineName, "No external engine");
  info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
  info.enabledExtensionCount = ext.size();
  info.enabledExtensionNames = ext.data();
  XrInstance result{};
  XR(xrCreateInstance(&info, &result));
  return result;
}
} // namespace
void XrSculpt::probe() {
  auto list = extensions();
  auto instance = create({});
  XrInstanceProperties p{XR_TYPE_INSTANCE_PROPERTIES};
  auto result = xrGetInstanceProperties(instance, &p);
  xrDestroyInstance(instance);
  check(result, "xrGetInstanceProperties");
  std::cout << "Runtime: " << p.runtimeName << '\n';
  for (auto name :
       {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_MNDX_EGL_ENABLE_EXTENSION_NAME,
        XR_EXTX_OVERLAY_EXTENSION_NAME})
    std::cout << name << ": " << (has(list, name) ? "yes" : "no") << '\n';
}
XrSculpt::XrSculpt(Document &value, std::atomic_bool *stopping, bool overlay)
    : document(value), stop(stopping) {
  try {
    setup(overlay);
    document.begin_spatial();
    renderer = std::make_unique<SurfaceRenderer>();
    palette = std::make_unique<SpatialPalette>();
    cursors = std::make_unique<CursorRenderer>();
  } catch (...) {
    cleanup();
    throw;
  }
}
XrSculpt::~XrSculpt() { cleanup(); }
void XrSculpt::cleanup() {
  for (auto &e : eyes) {
    glDeleteFramebuffers(1, &e.fbo);
    glDeleteRenderbuffers(1, &e.depth);
    if (e.swapchain)
      xrDestroySwapchain(e.swapchain);
  }
  eyes.clear();
  for (auto &space : handSpaces)
    if (space) {
      xrDestroySpace(space);
      space = XR_NULL_HANDLE;
    }
  for (auto &space : aimSpaces)
    if (space) {
      xrDestroySpace(space);
      space = XR_NULL_HANDLE;
    }
  if (headSpace) {
    xrDestroySpace(headSpace);
    headSpace = XR_NULL_HANDLE;
  }
  if (reference) {
    xrDestroySpace(reference);
    reference = XR_NULL_HANDLE;
  }
  if (session) {
    xrDestroySession(session);
    session = XR_NULL_HANDLE;
  }
  if (actionSet) {
    xrDestroyActionSet(actionSet);
    actionSet = XR_NULL_HANDLE;
  }
  if (instance) {
    xrDestroyInstance(instance);
    instance = XR_NULL_HANDLE;
  }
}
void XrSculpt::setup(bool overlay) {
  overlayMode = overlay;
  auto available = extensions();
  std::vector<const char *> ext{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
                                XR_MNDX_EGL_ENABLE_EXTENSION_NAME};
  for (auto name : ext)
    if (!has(available, name))
      throw std::runtime_error(std::string("Runtime must support ") + name);
  if (overlay) {
    if (!has(available, XR_EXTX_OVERLAY_EXTENSION_NAME))
      throw std::runtime_error("Overlay extension unavailable; --exclusive "
                               "explicitly allows primary XR session mode");
    ext.push_back(XR_EXTX_OVERLAY_EXTENSION_NAME);
  }
  std::vector<std::string> extensionNames;
  for (const auto &item : available)
    extensionNames.emplace_back(item.extensionName);
  auto profiles = controllerProfiles(extensionNames);
  bool ink = has(available, "XR_LOGITECH_mx_ink_stylus_interaction");
  if (ink)
    profiles.push_back({"/interaction_profiles/logitech/mx_ink_stylus_logitech",
                        "XR_LOGITECH_mx_ink_stylus_interaction", false});
  for (const auto &profile : profiles)
    if (!profile.extension.empty())
      ext.push_back(profile.extension.c_str());
  instance = create(ext);
  XrSystemGetInfo get{XR_TYPE_SYSTEM_GET_INFO};
  get.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XR(xrGetSystem(instance, &get, &system));
  PFN_xrGetOpenGLGraphicsRequirementsKHR reqFn{};
  XR(xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
                           reinterpret_cast<PFN_xrVoidFunction *>(&reqFn)));
  XrGraphicsRequirementsOpenGLKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
  XR(reqFn(instance, system, &req));
  GLint major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  auto version = XR_MAKE_VERSION(major, minor, 0);
  if (version < req.minApiVersionSupported ||
      version > req.maxApiVersionSupported)
    throw std::runtime_error("Unsupported OpenGL context version");
  XrSessionCreateInfoOverlayEXTX layer{
      XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX};
  layer.sessionLayersPlacement = 25;
  XrGraphicsBindingEGLMNDX binding{XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};
  binding.next = overlay ? &layer : nullptr;
  binding.display = graphics.display;
  binding.context = graphics.context;
  binding.config = graphics.config;
  binding.getProcAddress =
      reinterpret_cast<PFN_xrEglGetProcAddressMNDX>(eglGetProcAddress);
  XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};
  info.next = &binding;
  info.systemId = system;
  XR(xrCreateSession(instance, &info, &session));
  XrReferenceSpaceCreateInfo ref{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  uint32_t spaceCount = 0;
  XR(xrEnumerateReferenceSpaces(session, 0, &spaceCount, nullptr));
  std::vector<XrReferenceSpaceType> spaces(spaceCount);
  XR(xrEnumerateReferenceSpaces(session, spaceCount, &spaceCount,
                                spaces.data()));
  ref.referenceSpaceType =
      std::find(spaces.begin(), spaces.end(), XR_REFERENCE_SPACE_TYPE_STAGE) !=
              spaces.end()
          ? XR_REFERENCE_SPACE_TYPE_STAGE
          : XR_REFERENCE_SPACE_TYPE_LOCAL;
  std::cout << "XR reference="
            << (ref.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE
                    ? "STAGE"
                    : "LOCAL")
            << '\n';
  ref.poseInReferenceSpace.orientation.w = 1;
  reference_type_ = ref.referenceSpaceType;
  XR(xrCreateReferenceSpace(session, &ref, &reference));
  ref.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  XR(xrCreateReferenceSpace(session, &ref, &headSpace));
  XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};
  strcpy(set.actionSetName, "paint");
  strcpy(set.localizedActionSetName, "Painting");
  XR(xrCreateActionSet(instance, &set, &actionSet));
  XR(xrStringToPath(instance, "/user/hand/left", &hands[0]));
  XR(xrStringToPath(instance, "/user/hand/right", &hands[1]));
  auto action = [&](const char *name, XrActionType type, XrAction &handle) {
    XrActionCreateInfo a{XR_TYPE_ACTION_CREATE_INFO};
    strcpy(a.actionName, name);
    strcpy(a.localizedActionName, name);
    a.actionType = type;
    a.countSubactionPaths = 2;
    a.subactionPaths = hands;
    XR(xrCreateAction(actionSet, &a, &handle));
  };
  action("grip_pose", XR_ACTION_TYPE_POSE_INPUT, grip);
  action("aim_pose", XR_ACTION_TYPE_POSE_INPUT, aim);
  action("canvas_grab", XR_ACTION_TYPE_FLOAT_INPUT, squeeze);
  action("palette", XR_ACTION_TYPE_BOOLEAN_INPUT, menuAction);
  action("paint_pressure", XR_ACTION_TYPE_FLOAT_INPUT, trigger);
  action("tip_force", XR_ACTION_TYPE_FLOAT_INPUT, tipForce);
  action("undo", XR_ACTION_TYPE_BOOLEAN_INPUT, undo);
  action("redo", XR_ACTION_TYPE_BOOLEAN_INPUT, redo);
  action("eraser", XR_ACTION_TYPE_BOOLEAN_INPUT, eraserAction);
  action("left_stick", XR_ACTION_TYPE_VECTOR2F_INPUT, leftStick);
  action("right_stick", XR_ACTION_TYPE_VECTOR2F_INPUT, rightStick);
  action("mirror", XR_ACTION_TYPE_BOOLEAN_INPUT, mirrorAction);
  action("recenter", XR_ACTION_TYPE_BOOLEAN_INPUT, recenterAction);
  action("feedback", XR_ACTION_TYPE_VIBRATION_OUTPUT, haptic);
  const std::vector<std::pair<std::string, XrAction>> actions{
      {"grip", grip},
      {"aim", aim},
      {"trigger", trigger},
      {"tip_force", tipForce},
      {"squeeze", squeeze},
      {"menu", menuAction},
      {"undo", undo},
      {"redo", redo},
      {"eraser", eraserAction},
      {"left_stick", leftStick},
      {"right_stick", rightStick},
      {"mirror", mirrorAction},
      {"recenter", recenterAction},
      {"haptic", haptic}};
  for (const auto &profile : profiles) {
    std::vector<XrActionSuggestedBinding> bindings;
    auto suggested = controllerBindings(profile);
    if (profile.path.find("mx_ink") != std::string::npos) {
      suggested.clear();
      for (auto hand : {"left", "right"}) {
        std::string base = "/user/hand/" + std::string(hand) + "/";
        suggested.push_back({"grip", base + "input/tip_logitech/pose"});
        suggested.push_back({"aim", base + "input/aim/pose"});
        suggested.push_back(
            {"trigger", base + "input/cluster_middle_logitech/force"});
        suggested.push_back({"tip_force", base + "input/tip_logitech/force"});
        suggested.push_back({"haptic", base + "output/haptic"});
      }
    } else if (profile.path.find("index") != std::string::npos ||
               profile.path.find("vive") != std::string::npos) {
      bool index = profile.path.find("index") != std::string::npos;
      for (int i = 0; i < 2; ++i) {
        std::string path = std::string("/user/hand/") + (i ? "right" : "left") +
                           "/input/" + (index ? "thumbstick" : "trackpad");
        suggested.push_back({i ? "right_stick" : "left_stick", path});
        suggested.push_back({i ? "recenter" : "mirror", path + "/click"});
      }
    }
    for (const auto &binding : suggested) {
      auto action =
          std::find_if(actions.begin(), actions.end(), [&](const auto &a) {
            return a.first == binding.action;
          });
      if (action == actions.end())
        throw std::runtime_error("Unknown controller action");
      XrPath path;
      XR(xrStringToPath(instance, binding.path.c_str(), &path));
      bindings.push_back({action->second, path});
    }
    XrInteractionProfileSuggestedBinding suggest{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    XR(xrStringToPath(instance, profile.path.c_str(),
                      &suggest.interactionProfile));
    suggest.countSuggestedBindings = bindings.size();
    suggest.suggestedBindings = bindings.data();
    if (XR_FAILED(xrSuggestInteractionProfileBindings(instance, &suggest)))
      std::cerr << "Controller profile not available: " << profile.path << '\n';
  }
  XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach.countActionSets = 1;
  attach.actionSets = &actionSet;
  XR(xrAttachSessionActionSets(session, &attach));
  for (int i = 0; i < 2; ++i) {
    XrActionSpaceCreateInfo space{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    space.action = grip;
    space.subactionPath = hands[i];
    space.poseInActionSpace.orientation.w = 1;
    XR(xrCreateActionSpace(session, &space, &handSpaces[i]));
    space.action = aim;
    XR(xrCreateActionSpace(session, &space, &aimSpaces[i]));
  }
  uint32_t n = 0;
  XR(xrEnumerateEnvironmentBlendModes(instance, system,
                                      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                      0, &n, nullptr));
  std::vector<XrEnvironmentBlendMode> modes(n);
  XR(xrEnumerateEnvironmentBlendModes(instance, system,
                                      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                      n, &n, modes.data()));
  if (modes.empty())
    throw std::runtime_error("No environment blend modes");
  blend = std::find(modes.begin(), modes.end(),
                    XR_ENVIRONMENT_BLEND_MODE_OPAQUE) != modes.end()
              ? XR_ENVIRONMENT_BLEND_MODE_OPAQUE
              : modes[0];
  XR(xrEnumerateViewConfigurationViews(
      instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n,
      nullptr));
  if (n != 2)
    throw std::runtime_error("Stereo view configuration required");
  std::vector<XrViewConfigurationView> views(n,
                                             {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  XR(xrEnumerateViewConfigurationViews(
      instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, &n,
      views.data()));
  uint32_t count;
  XR(xrEnumerateSwapchainFormats(session, 0, &count, nullptr));
  std::vector<int64_t> formats(count);
  XR(xrEnumerateSwapchainFormats(session, count, &count, formats.data()));
  int64_t format = std::find(formats.begin(), formats.end(), GL_SRGB8_ALPHA8) !=
                           formats.end()
                       ? GL_SRGB8_ALPHA8
                       : GL_RGBA8;
  if (std::find(formats.begin(), formats.end(), format) == formats.end())
    throw std::runtime_error("RGBA swapchains unavailable");
  eyes.resize(n);
  for (size_t i = 0; i < n; ++i) {
    auto &e = eyes[i];
    e.width = views[i].recommendedImageRectWidth;
    e.height = views[i].recommendedImageRectHeight;
    XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    sc.format = format;
    sc.sampleCount = 1;
    sc.width = e.width;
    sc.height = e.height;
    sc.faceCount = 1;
    sc.arraySize = 1;
    sc.mipCount = 1;
    XR(xrCreateSwapchain(session, &sc, &e.swapchain));
    XR(xrEnumerateSwapchainImages(e.swapchain, 0, &count, nullptr));
    e.images.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
    XR(xrEnumerateSwapchainImages(
        e.swapchain, count, &count,
        reinterpret_cast<XrSwapchainImageBaseHeader *>(e.images.data())));
    glGenFramebuffers(1, &e.fbo);
    glGenRenderbuffers(1, &e.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, e.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, e.width,
                          e.height);
  }
}
void XrSculpt::finishStroke() { document.finish_spatial(); }
void XrSculpt::input(XrTime time) {
  SpatialFrame frame;
  frame.sequence = ++sequence;
  frame.time = double(time) / 1e9;
  frame.focused = state == XR_SESSION_STATE_FOCUSED;
  if (!frame.focused) {
    document.submit(frame);
    return;
  }
  XrActiveActionSet active{actionSet, XR_NULL_PATH};
  XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
  sync.countActiveActionSets = 1;
  sync.activeActionSets = &active;
  XR(xrSyncActions(session, &sync));
  auto boolean = [&](XrAction action, int hand, bool edge = false) {
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
    get.action = action;
    get.subactionPath = hands[hand];
    XrActionStateBoolean value{XR_TYPE_ACTION_STATE_BOOLEAN};
    XR(xrGetActionStateBoolean(session, &get, &value));
    return bool(value.isActive && value.currentState &&
                (!edge || value.changedSinceLastSync));
  };
  auto amount = [&](XrAction action, int hand) {
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
    get.action = action;
    get.subactionPath = hands[hand];
    XrActionStateFloat value{XR_TYPE_ACTION_STATE_FLOAT};
    XR(xrGetActionStateFloat(session, &get, &value));
    return value.isActive && std::isfinite(value.currentState)
               ? std::clamp(value.currentState, 0.f, 1.f)
               : 0.f;
  };
  auto pose = [&](XrSpace space, CanvasTransform &target) {
    XrSpaceLocation p{XR_TYPE_SPACE_LOCATION};
    XR(xrLocateSpace(space, reference, time, &p));
    auto flags = XR_SPACE_LOCATION_POSITION_VALID_BIT |
                 XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if ((p.locationFlags & flags) != flags)
      return false;
    target = {{p.pose.position.x, p.pose.position.y, p.pose.position.z},
              {p.pose.orientation.x, p.pose.orientation.y, p.pose.orientation.z,
               p.pose.orientation.w},
              1};
    auto q = target.orientation;
    float norm = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    return ob::finite(target) && std::abs(norm - 1) < .01f;
  };
  auto active_pose = [&](XrAction action, int hand) {
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
    get.action = action;
    get.subactionPath = hands[hand];
    XrActionStatePose p{XR_TYPE_ACTION_STATE_POSE};
    XR(xrGetActionStatePose(session, &get, &p));
    return bool(p.isActive);
  };
  auto axes = [&](XrAction action, int hand) {
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
    get.action = action;
    get.subactionPath = hands[hand];
    XrActionStateVector2f a{XR_TYPE_ACTION_STATE_VECTOR2F};
    XR(xrGetActionStateVector2f(session, &get, &a));
    return a.isActive ? a.currentState : XrVector2f{};
  };
  frame.head_valid = pose(headSpace, frame.head);
  for (int i = 0; i < 2; ++i) {
    auto &h = frame.hands[i];
    h.tracked.valid =
        active_pose(grip, i) && pose(handSpaces[i], h.tracked.grip);
    h.tracked.aimValid =
        active_pose(aim, i) && pose(aimSpaces[i], h.tracked.aim);
    h.tracked.timeMs = uint32_t(time / 1000000);
    h.tracked.trigger = std::max(amount(trigger, i), amount(tipForce, i));
    h.tracked.squeeze = amount(squeeze, i);
    h.kind =
        stylusHands[i] ? DeviceKind::SpatialStylus : DeviceKind::Controller;
    h.joystick =
        boolean(i ? recenterAction : mirrorAction, i) || boolean(menuAction, i);
    auto a = axes(i ? rightStick : leftStick, i);
    h.x = a.x;
    h.y = a.y;
    h.undo = boolean(undo, i, true);
    h.redo = boolean(redo, i, true);
    if (h.tracked.valid)
      ++trackedFrames[i];
  }
  document.submit(std::move(frame));
}
void XrSculpt::events() {
  XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
  XrResult result;
  while ((result = xrPollEvent(instance, &event)) == XR_SUCCESS) {
    if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
      state = reinterpret_cast<XrEventDataSessionStateChanged &>(event).state;
      if (state == XR_SESSION_STATE_READY) {
        XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
        begin.primaryViewConfigurationType =
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XR(xrBeginSession(session, &begin));
        running = true;
      } else if (state == XR_SESSION_STATE_STOPPING) {
        finishStroke();
        XR(xrEndSession(session));
        running = false;
      } else if (state == XR_SESSION_STATE_EXITING ||
                 state == XR_SESSION_STATE_LOSS_PENDING)
        quit = true;
    } else if (event.type ==
               XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
      auto &change =
          reinterpret_cast<XrEventDataReferenceSpaceChangePending &>(event);
      if (change.referenceSpaceType == reference_type_) {
        std::optional<CanvasTransform> delta;
        if (change.poseValid) {
          auto &p = change.poseInPreviousSpace;
          delta = CanvasTransform{{p.position.x, p.position.y, p.position.z},
                                  {p.orientation.x, p.orientation.y,
                                   p.orientation.z, p.orientation.w},
                                  1};
        }
        if (changes_.size() >= 64)
          throw std::runtime_error(
              "Too many pending XR reference-space changes");
        changes_.push_back({change.changeTime, delta});
        std::stable_sort(changes_.begin(), changes_.end(),
                         [](auto &a, auto &b) { return a.time < b.time; });
      }
    } else if (event.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
      for (int i = 0; i < 2; ++i) {
        XrInteractionProfileState profile{XR_TYPE_INTERACTION_PROFILE_STATE};
        XR(xrGetCurrentInteractionProfile(session, hands[i], &profile));
        stylusHands[i] = false;
        if (profile.interactionProfile != XR_NULL_PATH) {
          char name[XR_MAX_PATH_LENGTH];
          uint32_t length = 0;
          XR(xrPathToString(instance, profile.interactionProfile, sizeof(name),
                            &length, name));
          stylusHands[i] =
              std::string(name).find("mx_ink") != std::string::npos;
          std::cout << "XR " << (i == 0 ? "left" : "right")
                    << " profile=" << name << '\n';
        }
      }
    } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
      quit = true;
    event = {XR_TYPE_EVENT_DATA_BUFFER};
  }
  if (result != XR_EVENT_UNAVAILABLE)
    check(result, "xrPollEvent");
}
void XrSculpt::run(unsigned frameLimit) {
  unsigned frames = 0, rendered = 0, paletteFrames = 0;
  while (!quit && !(stop && stop->load())) {
    events();
    if (!running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frame{XR_TYPE_FRAME_STATE};
    XR(xrWaitFrame(session, &wait, &frame));
    XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
    XR(xrBeginFrame(session, &begin));
    while (!changes_.empty() &&
           changes_.front().time <= frame.predictedDisplayTime) {
      document.rebase_space(changes_.front().old_from_new);
      changes_.erase(changes_.begin());
    }
    input(frame.predictedDisplayTime);
    auto content = document.frame();
    std::vector<XrCompositionLayerProjectionView> layers(
        eyes.size(), {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = reference;
    layer.layerFlags =
        overlayMode ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
    layer.viewCount = layers.size();
    layer.views = layers.data();
    bool submit = false;
    if (frame.shouldRender) {
      XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
      locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
      locate.displayTime = frame.predictedDisplayTime;
      locate.space = reference;
      XrViewState viewState{XR_TYPE_VIEW_STATE};
      std::vector<XrView> views(eyes.size(), {XR_TYPE_VIEW});
      uint32_t n;
      XR(xrLocateViews(session, &locate, &viewState, views.size(), &n,
                       views.data()));
      auto flags = XR_VIEW_STATE_POSITION_VALID_BIT |
                   XR_VIEW_STATE_ORIENTATION_VALID_BIT;
      if (n == eyes.size() && (viewState.viewStateFlags & flags) == flags) {
        submit = true;
        for (size_t i = 0; i < eyes.size(); ++i) {
          auto &e = eyes[i];
          uint32_t index;
          XrSwapchainImageAcquireInfo acq{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
          XR(xrAcquireSwapchainImage(e.swapchain, &acq, &index));
          XrSwapchainImageWaitInfo iw{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
          iw.timeout = XR_INFINITE_DURATION;
          XR(xrWaitSwapchainImage(e.swapchain, &iw));
          glBindFramebuffer(GL_FRAMEBUFFER, e.fbo);
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D, e.images[index].image, 0);
          glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                    GL_RENDERBUFFER, e.depth);
          if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
              GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("XR framebuffer incomplete");
          auto &p = views[i].pose;
          auto &f = views[i].fov;
          auto vp = multiply(
              projection(f.angleLeft, f.angleRight, f.angleDown, f.angleUp),
              view_matrix({p.position.x, p.position.y, p.position.z},
                          {p.orientation.x, p.orientation.y, p.orientation.z,
                           p.orientation.w}));
          renderer->draw(content.surface, content.state->canvas, vp,
                         {p.position.x, p.position.y, p.position.z}, e.width,
                         e.height, overlayMode, content.state->toon,
                         content.state->light);
          cursors->draw(*content.state, vp);
          palette->draw(*content.state, vp);
          glFlush();
          XrSwapchainImageReleaseInfo release{
              XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
          XR(xrReleaseSwapchainImage(e.swapchain, &release));
          layers[i].pose = p;
          layers[i].fov = f;
          layers[i].subImage.swapchain = e.swapchain;
          layers[i].subImage.imageRect.extent = {e.width, e.height};
        }
      }
    }
    const XrCompositionLayerBaseHeader *base =
        reinterpret_cast<const XrCompositionLayerBaseHeader *>(&layer);
    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
    end.displayTime = frame.predictedDisplayTime;
    end.environmentBlendMode = blend;
    end.layerCount = submit ? 1 : 0;
    end.layers = submit ? &base : nullptr;
    XR(xrEndFrame(session, &end));
    if (submit && content.state->menu)
      ++paletteFrames;
    if (submit)
      ++rendered;
    ++frames;
    if (frameLimit && frames >= frameLimit)
      break;
  }
  finishStroke();
  std::cout << "XR frames=" << frames << " stereo_submissions=" << rendered
            << " palette_frames=" << paletteFrames
            << " tracked_left=" << trackedFrames[0]
            << " tracked_right=" << trackedFrames[1] << "\n";
}
} // namespace dome
