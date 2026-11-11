// SPDX-License-Identifier: GPL-3.0-or-later
// Experimental host glue; adapted shader provenance in NOTICE.txt.
#pragma once
#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>
#include <cstddef>
namespace armorpaint_proof {
// Current caller-owned Dome OpenGL context; no window/engine/event-loop owner.
// Input RGB is linear, unassociated data (also admits physical radiance >1),
// alpha is an independent FLOAT coverage; mask is independent [0,1] R32F.
// Output is associated RGB + FLOAT coverage. This API does not touch documents,
// normal-frame authority, source roles, rig/morph/VRM data, or history.
class Blur {
 public:
  Blur(); ~Blur();
  Blur(const Blur&)=delete; Blur& operator=(const Blur&)=delete;
  void run(GLuint source,GLuint mask,GLuint output,float strength,
           std::size_t byte_budget=4*1024*1024);
 private:
  GLuint program_=0,framebuffer_=0,vao_=0;
};
}
