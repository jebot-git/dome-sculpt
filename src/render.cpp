// SPDX-License-Identifier: GPL-3.0-or-later
// Matrix helpers adapted from OpenBrush Native (Apache-2.0).
#include "dome/render.hpp"
#include <cstddef>
namespace dome {
Mat4 model_matrix(CanvasTransform t) {
  auto x = ob::rotate(t.orientation, {t.scale, 0, 0}),
       y = ob::rotate(t.orientation, {0, t.scale, 0}),
       z = ob::rotate(t.orientation, {0, 0, t.scale});
  return {x.x, x.y, x.z, 0, y.x,          y.y,          y.z,          0,
          z.x, z.y, z.z, 0, t.position.x, t.position.y, t.position.z, 1};
}
Mat4 view_matrix(ob::Vec3 p, ob::Quat q) {
  auto inv = ob::conjugate(q);
  auto result = model_matrix({ob::rotate(inv, p) * -1, inv, 1});
  return result;
}
Mat4 multiply(const Mat4 &a, const Mat4 &b) {
  Mat4 c{};
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row)
      for (int i = 0; i < 4; ++i)
        c[col * 4 + row] += a[i * 4 + row] * b[col * 4 + i];
  return c;
}
Mat4 projection(float l, float r, float b, float t, float n, float f) {
  l = std::tan(l);
  r = std::tan(r);
  b = std::tan(b);
  t = std::tan(t);
  return {2 / (r - l),
          0,
          0,
          0,
          0,
          2 / (t - b),
          0,
          0,
          (r + l) / (r - l),
          (t + b) / (t - b),
          -(f + n) / (f - n),
          -1,
          0,
          0,
          -2 * f * n / (f - n),
          0};
}
namespace {
GLuint shader(GLenum kind, const char *text) {
  GLuint s = glCreateShader(kind);
  glShaderSource(s, 1, &text, nullptr);
  glCompileShader(s);
  GLint good = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &good);
  if (!good) {
    char log[4096];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    glDeleteShader(s);
    throw std::runtime_error(log);
  }
  return s;
}
} // namespace
SurfaceRenderer::SurfaceRenderer() {
  const char *vertex = R"(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;
uniform mat4 model,mvp;out vec3 world,n;
void main(){vec4 p=model*vec4(position,1);world=p.xyz;n=normalize(mat3(model)*normal);gl_Position=mvp*vec4(position,1);})";
  const char *fragment = R"(#version 330 core
in vec3 world,n;out vec4 color;uniform vec3 camera;uniform int toon;uniform vec4 base_color;uniform float metallic,roughness;uniform vec3 emission;
const float PI=3.141592653589793;
vec3 shade(vec3 N,vec3 V,vec3 L,vec3 power){
vec3 H=normalize(V+L);float NV=max(dot(N,V),0.0001),NL=max(dot(N,L),0.0),NH=max(dot(N,H),0.0),VH=max(dot(V,H),0.0);
vec3 base=base_color.rgb;float rough=max(.04,roughness),a=rough*rough,a2=a*a;
float d=a2/(PI*pow(NH*NH*(a2-1.0)+1.0,2.0));float k=pow(rough+1.0,2.0)/8.0;
float g=NV/(NV*(1.0-k)+k)*NL/(NL*(1.0-k)+k);vec3 F0=mix(vec3(.04),base,metallic);vec3 F=F0+(1.0-F0)*pow(1.0-VH,5.0);
return ((1.0-F)*(1.0-metallic)*base/PI+d*g*F/max(4.0*NV*NL,.0001))*power*NL;}
void main(){vec3 N=normalize(n)*(gl_FrontFacing?1.0:-1.0),V=normalize(camera-world),result;
if(toon==1){float lit=dot(N,normalize(vec3(-.4,.8,.7)));float ramp=mix(.32,1.0,smoothstep(-.05,.05,lit));float edge=smoothstep(.05,.18,max(dot(N,V),0.0));result=base_color.rgb*ramp*edge+vec3(.025);}
else result=shade(N,V,normalize(vec3(-.4,.8,.7)),vec3(3.8))+shade(N,V,normalize(vec3(.7,.2,-.4)),vec3(.65,.8,1.0))+.045*base_color.rgb;
result+=emission;result=result/(result+vec3(1));color=vec4(pow(result,vec3(1.0/2.2)),base_color.a);})";
  GLuint vs = shader(GL_VERTEX_SHADER, vertex),
         fs = shader(GL_FRAGMENT_SHADER, fragment);
  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint good = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &good);
  if (!good)
    throw std::runtime_error("Sculpt shader linking failed");
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vertices_);
  glGenBuffers(1, &indices_);
}
SurfaceRenderer::~SurfaceRenderer() {
  glDeleteBuffers(1, &indices_);
  glDeleteBuffers(1, &vertices_);
  glDeleteVertexArrays(1, &vao_);
  glDeleteProgram(program_);
}
void SurfaceRenderer::draw(std::shared_ptr<const MeshSnapshot> mesh,
                           CanvasTransform model, const Mat4 &vp,
                           ob::Vec3 camera, int width, int height,
                           bool transparent, bool toon, bool light) {
  glViewport(0, 0, width, height);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  float background = transparent ? 0.f : light ? .75f : .12f;
  glClearColor(background, background, background, transparent ? 0 : 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (mesh != cached_) {
    struct Vertex {
      Vec3 p, n;
    };
    std::vector<Vertex> v;
    v.reserve(mesh->mesh.positions.size());
    for (size_t i = 0; i < mesh->mesh.positions.size(); ++i)
      v.push_back({mesh->mesh.positions[i], mesh->mesh.normals[i]});
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vertices_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, n)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mesh->mesh.indices.size() * sizeof(uint32_t),
                 mesh->mesh.indices.data(), GL_STATIC_DRAW);
    count_ = mesh->mesh.indices.size();
    cached_ = std::move(mesh);
  }
  glUseProgram(program_);
  auto m = model_matrix(model), mvp = multiply(vp, m);
  glUniformMatrix4fv(glGetUniformLocation(program_, "model"), 1, GL_FALSE,
                     m.data());
  glUniformMatrix4fv(glGetUniformLocation(program_, "mvp"), 1, GL_FALSE,
                     mvp.data());
  glUniform3f(glGetUniformLocation(program_, "camera"), camera.x, camera.y,
              camera.z);
  glBindVertexArray(vao_);
  auto draw_range = [&](const Material &material, size_t first, size_t count) {
    glUniform1i(glGetUniformLocation(program_, "toon"), toon || material.toon);
    glUniform4fv(glGetUniformLocation(program_, "base_color"), 1,
                 material.color.data());
    glUniform3fv(glGetUniformLocation(program_, "emission"), 1,
                 material.emissive.data());
    glUniform1f(glGetUniformLocation(program_, "metallic"), material.metallic);
    glUniform1f(glGetUniformLocation(program_, "roughness"),
                material.roughness);
    if (material.double_sided)
      glDisable(GL_CULL_FACE);
    else {
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);
    }
    if (material.color[3] < 1) {
      glEnable(GL_BLEND);
      glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                          GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
    } else {
      glDisable(GL_BLEND);
      glDepthMask(GL_TRUE);
    }
    glDrawElements(GL_TRIANGLES, GLsizei(count), GL_UNSIGNED_INT,
                   reinterpret_cast<void *>(first * sizeof(uint32_t)));
  };
  if (cached_->ranges.empty())
    draw_range(Material{}, 0, count_);
  else {
    for (auto &r : cached_->ranges)
      if (r.material.color[3] >= 1)
        draw_range(r.material, r.first, r.count);
    for (auto &r : cached_->ranges)
      if (r.material.color[3] < 1)
        draw_range(r.material, r.first, r.count);
  }
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(0);
  glUseProgram(0);
  if (glGetError() != GL_NO_ERROR)
    throw std::runtime_error("Sculpt GPU rendering failed");
}

CursorRenderer::CursorRenderer() {
  const char *v = R"(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 color;uniform mat4 vp;out vec3 tint;void main(){tint=color;gl_Position=vp*vec4(position,1);})";
  const char *f = R"(#version 330 core
in vec3 tint;out vec4 color;void main(){color=vec4(tint,1);})";
  auto vs = shader(GL_VERTEX_SHADER, v), fs = shader(GL_FRAGMENT_SHADER, f);
  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint good = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &good);
  if (!good)
    throw std::runtime_error("Cursor shader failed");
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &buffer_);
}
CursorRenderer::~CursorRenderer() {
  glDeleteBuffers(1, &buffer_);
  glDeleteVertexArrays(1, &vao_);
  glDeleteProgram(program_);
}
void CursorRenderer::draw(const EditorState &state, const Mat4 &vp) {
  struct Vertex {
    Vec3 p, color;
  };
  std::vector<Vertex> vertices;
  auto line = [&](ob::Vec3 a, ob::Vec3 b, Vec3 color) {
    vertices.push_back({{a.x, a.y, a.z}, color});
    vertices.push_back({{b.x, b.y, b.z}, color});
  };
  for (auto &h : state.hands)
    if (h.tracked.valid) {
      auto p = h.tracked.grip.position;
      auto axis = ob::rotate(h.tracked.grip.orientation, {0, 0, -.08f});
      line(p, p + axis, {.9f, .75f, .2f});
      if (!state.menu) {
        float r =
            state.radius * (h.tracked.trigger > .02f
                                ? .25f + .75f * std::sqrt(h.tracked.trigger)
                                : 1.f);
        Vec3 color =
            state.tool == Tool::Carve ? Vec3{1, .2f, .2f} : Vec3{.1f, .9f, .5f};
        for (int axis = 0; axis < 3; ++axis)
          for (int k = 0; k < 32; ++k) {
            float a = k * 6.283185307f / 32, b = (k + 1) * 6.283185307f / 32;
            auto point = [&](float t) {
              float x = std::cos(t) * r, y = std::sin(t) * r;
              return p + (axis == 0   ? ob::Vec3{x, y, 0}
                          : axis == 1 ? ob::Vec3{x, 0, y}
                                      : ob::Vec3{0, x, y});
            };
            line(point(a), point(b), color);
          }
      }
    }
  if (vertices.empty())
    return;
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glUseProgram(program_);
  glUniformMatrix4fv(glGetUniformLocation(program_, "vp"), 1, GL_FALSE,
                     vp.data());
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STREAM_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, color)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glDrawArrays(GL_LINES, 0, vertices.size());
  glBindVertexArray(0);
  glUseProgram(0);
  glDepthMask(GL_TRUE);
}
} // namespace dome
