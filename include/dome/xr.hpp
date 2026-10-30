// SPDX-License-Identifier: GPL-3.0-or-later
// Native session/swapchain lifecycle adapted from OpenBrush Native, Apache-2.0.
#pragma once
#include "controller-profiles.h"
#include "graphics.h"
#include "palette.hpp"
#include "polygon-render.hpp"
#define XR_USE_PLATFORM_EGL
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
namespace dome {
class XrSculpt {
  struct Eye {
    XrSwapchain swapchain{};
    int width = 0, height = 0;
    GLuint fbo = 0, depth = 0;
    std::vector<XrSwapchainImageOpenGLKHR> images;
  };
  NativeGraphics graphics;
  Document &document;
  std::atomic_bool *stop = nullptr;
  XrInstance instance{};
  XrSystemId system{};
  XrSession session{};
  XrSpace reference{}, headSpace{};
  XrActionSet actionSet{};
  XrAction grip{}, aim{}, trigger{}, tipForce{}, squeeze{}, menuAction{},
      undo{}, redo{}, eraserAction{}, leftStick{}, rightStick{}, mirrorAction{},
      recenterAction{}, haptic{};
  XrPath hands[2]{};
  XrSpace handSpaces[2]{}, aimSpaces[2]{};
  std::vector<Eye> eyes;
  XrSessionState state = XR_SESSION_STATE_UNKNOWN;
  bool running = false, quit = false, overlayMode = true;
  SurfaceOutputEncoding output_encoding_ = SurfaceOutputEncoding::Linear;
  std::unique_ptr<SurfaceRenderer> renderer;
  std::unique_ptr<PolygonRenderer> polygons;
  std::unique_ptr<SpatialPalette> palette;
  std::unique_ptr<CursorRenderer> cursors;
  bool stylusHands[2]{};
  uint64_t sequence = 0, trackedFrames[2]{};
  struct SpaceChange {
    XrTime time;
    std::optional<CanvasTransform> old_from_new;
  };
  std::vector<SpaceChange> changes_;
  XrReferenceSpaceType reference_type_ = XR_REFERENCE_SPACE_TYPE_LOCAL;
  XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  void setup(bool overlay);
  void events();
  void input(XrTime);
  void finishStroke();
  void cleanup();

public:
  XrSculpt(Document &, std::atomic_bool *stop = nullptr, bool overlay = true);
  ~XrSculpt();
  void run(unsigned frames = 0);
  static void probe();
};
} // namespace dome
