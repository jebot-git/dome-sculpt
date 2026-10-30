// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-render.hpp"
#include <limits>
namespace dome {
namespace {
GLuint compile(GLenum type, const char *text) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &text, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char error[4096]{};
    glGetShaderInfoLog(shader, sizeof(error), nullptr, error);
    glDeleteShader(shader);
    throw std::runtime_error(error);
  }
  return shader;
}
float linear(float x) {
  return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f);
}
struct GLState {
  GLint program = 0, vao = 0, array = 0, depth_function = 0;
  GLboolean depth_mask;
  bool depth, blend, cull, offset, points, srgb;
  GLfloat factor, units, point_size, line_width;
  GLState() {
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array);
    glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
    depth = glIsEnabled(GL_DEPTH_TEST);
    blend = glIsEnabled(GL_BLEND);
    cull = glIsEnabled(GL_CULL_FACE);
    offset = glIsEnabled(GL_POLYGON_OFFSET_FILL);
    points = glIsEnabled(GL_PROGRAM_POINT_SIZE);
    srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &factor);
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &units);
    glGetFloatv(GL_POINT_SIZE, &point_size);
    glGetFloatv(GL_LINE_WIDTH, &line_width);
  }
  ~GLState() {
    auto restore = [](GLenum name, bool value) {
      if (value)
        glEnable(name);
      else
        glDisable(name);
    };
    glUseProgram(program);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, array);
    restore(GL_DEPTH_TEST, depth);
    restore(GL_BLEND, blend);
    restore(GL_CULL_FACE, cull);
    restore(GL_POLYGON_OFFSET_FILL, offset);
    restore(GL_PROGRAM_POINT_SIZE, points);
    restore(GL_FRAMEBUFFER_SRGB, srgb);
    glDepthFunc(depth_function);
    glDepthMask(depth_mask);
    glPolygonOffset(factor, units);
    glPointSize(point_size);
    glLineWidth(line_width);
  }
};
template <class T> size_t bytes(const std::vector<T> &v) {
  if (v.size() > size_t(std::numeric_limits<GLsizeiptr>::max()) / sizeof(T))
    throw std::length_error("Polygon GPU store exceeds GL address capacity");
  return v.size() * sizeof(T);
}
void indices(const std::vector<uint32_t> &v, size_t vertices) {
  if (v.size() > size_t(std::numeric_limits<GLsizei>::max()))
    throw std::length_error("Polygon GPU draw exceeds its index capacity");
  for (auto i : v)
    if (i >= vertices)
      throw std::invalid_argument(
          "Polygon GPU index is outside its immutable vertex domain");
}
GLuint upload(GLenum target, size_t size, const void *data) {
  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  glBindBuffer(target, buffer);
  glBufferData(target, GLsizeiptr(size), data, GL_STATIC_DRAW);
  if (glGetError() != GL_NO_ERROR) {
    glDeleteBuffers(1, &buffer);
    throw std::runtime_error("Unable to allocate polygon GPU buffers");
  }
  return buffer;
}
} // namespace
struct PolygonRenderer::Stores {
  GLuint vao = 0, positions = 0, normals = 0, triangles = 0, wires = 0,
         weights = 0, normal_coefficients = 0, selected_edges = 0,
         selected_points = 0;
  bool own_geometry = true;
  std::shared_ptr<const PolygonRender> geometry;
  std::shared_ptr<const PolygonDrawData> selection;
  ~Stores() {
    if (vao)
      glDeleteVertexArrays(1, &vao);
    for (auto name :
         {weights, normal_coefficients, selected_edges, selected_points})
      if (name)
        glDeleteBuffers(1, &name);
    if (own_geometry)
      for (auto name : {positions, normals, triangles, wires})
        if (name)
          glDeleteBuffers(1, &name);
  }
  void abandon() {
    vao = positions = normals = triangles = wires = weights =
        normal_coefficients = selected_edges = selected_points = 0;
  }
};
PolygonRenderer::PolygonRenderer() {
  static const char *vertex = R"GLSL(#version 330 core
 layout(location=0) in vec3 position;
 layout(location=1) in vec3 normal;
 layout(location=2) in float selected;
 layout(location=3) in vec3 normal_coefficient;
 uniform mat4 object_to_clip,object_to_world;
 uniform vec3 delta;
 uniform float point_size;
 out vec3 world_normal,world_position;
 void main(){vec3 p=position+delta*selected;gl_Position=object_to_clip*vec4(p,1);world_position=(object_to_world*vec4(p,1)).xyz;vec3 n=normal+cross(delta,normal_coefficient);world_normal=mat3(object_to_world)*(dot(n,n)>0?n:normal);gl_PointSize=point_size;}
 )GLSL";
  static const char *fragment = R"GLSL(#version 330 core
 in vec3 world_normal,world_position;
 uniform int pass;
 uniform bool linear_output,moving;
 uniform vec3 accent;
 out vec4 output_color;
 vec3 display_rgb(vec3 c){return mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1./2.4))-.055,greaterThan(c,vec3(.0031308)));}
 void main(){vec3 color;
 if(pass==0){vec3 n=normalize(world_normal);if(!gl_FrontFacing)n=-n;float light=.27+.73*max(dot(n,normalize(vec3(.35,.7,.6))),0.);color=vec3(.20,.25,.32)*light;}
 else color=pass==1?vec3(.018):accent;
 output_color=vec4(linear_output?color:display_rgb(color),1);
 }
 )GLSL";
  GLuint vs = 0, fs = 0;
  try {
    vs = compile(GL_VERTEX_SHADER, vertex);
    fs = compile(GL_FRAGMENT_SHADER, fragment);
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
      char error[4096]{};
      glGetProgramInfoLog(program_, sizeof(error), nullptr, error);
      throw std::runtime_error(error);
    }
    glDeleteShader(fs);
    fs = 0;
    fs = compile(GL_FRAGMENT_SHADER, R"GLSL(#version 330 core
      uniform bool linear_output;
      uniform vec3 accent;
      out vec4 output_color;
      vec3 display_rgb(vec3 c){return mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1./2.4))-.055,greaterThan(c,vec3(.0031308)));}
      void main(){if(dot(gl_PointCoord-vec2(.5),gl_PointCoord-vec2(.5))>.25)discard;output_color=vec4(linear_output?accent:display_rgb(accent),1);}
    )GLSL");
    points_program_ = glCreateProgram();
    glAttachShader(points_program_, vs);
    glAttachShader(points_program_, fs);
    glLinkProgram(points_program_);
    glGetProgramiv(points_program_, GL_LINK_STATUS, &ok);
    if (!ok)
      throw std::runtime_error("Unable to link polygon vertex marker program");
    glDeleteShader(vs);
    glDeleteShader(fs);
  } catch (...) {
    if (vs)
      glDeleteShader(vs);
    if (fs)
      glDeleteShader(fs);
    if (program_)
      glDeleteProgram(program_);
    if (points_program_)
      glDeleteProgram(points_program_);
    program_ = points_program_ = 0;
    throw;
  }
}
PolygonRenderer::~PolygonRenderer() {
  stores_.reset();
  if (points_program_)
    glDeleteProgram(points_program_);
  if (program_)
    glDeleteProgram(program_);
}
void PolygonRenderer::abandon() {
  if (stores_)
    stores_->abandon();
  stores_.reset();
  program_ = points_program_ = 0;
}
void PolygonRenderer::draw(const PolygonView &view,
                           const std::array<float, 16> &clip,
                           const std::array<float, 16> &world, uint32_t accent,
                           bool linear_output, float point_size) {
  if (!view.surface || !view.display)
    throw std::invalid_argument("Polygon preview has no immutable render data");
  if (!finite(view.move_delta) || !std::isfinite(point_size) ||
      point_size <= 0 || point_size > 64)
    throw std::invalid_argument("Invalid polygon GPU preview state");
  for (auto x : clip)
    if (!std::isfinite(x))
      throw std::invalid_argument("Invalid polygon clip matrix");
  for (auto x : world)
    if (!std::isfinite(x))
      throw std::invalid_argument("Invalid polygon world matrix");
  if (!view.moving && (view.move_delta.x != 0 || view.move_delta.y != 0 ||
                       view.move_delta.z != 0))
    throw std::invalid_argument(
        "Polygon displacement requires an active move preview");
  static_assert(sizeof(Vec3) == 3 * sizeof(float));
  GLState restore;
  const auto &mesh = view.surface->mesh;
  const auto &display = *view.display;
  bool geometry = !stores_ || stores_->geometry != view.surface,
       selection = geometry || stores_->selection != view.display;
  if (selection) {
    if (mesh.positions.size() != mesh.normals.size() ||
        display.move_weights.size() != mesh.positions.size() ||
        display.normal_coefficients.size() != mesh.positions.size())
      throw std::invalid_argument("Polygon GPU attribute domains disagree");
    for (auto p : mesh.positions)
      if (!finite(p))
        throw std::invalid_argument("Invalid polygon GPU position");
    for (auto n : mesh.normals)
      if (!finite(n))
        throw std::invalid_argument("Invalid polygon GPU normal");
    for (auto c : display.normal_coefficients)
      if (!finite(c))
        throw std::invalid_argument("Invalid polygon normal coefficient");
    for (float w : display.move_weights)
      if (w != 0 && w != 1)
        throw std::invalid_argument("Invalid polygon vertex selection mask");
    indices(mesh.indices, mesh.positions.size());
    indices(display.wire_indices, mesh.positions.size());
    indices(display.selected_edges, mesh.positions.size());
    indices(display.selected_points, mesh.positions.size());
    if (mesh.indices.size() % 3 || display.wire_indices.size() % 2 ||
        display.selected_edges.size() % 2)
      throw std::invalid_argument("Invalid polygon GPU primitive domain");
    if (!geometry && display.wire_indices != stores_->selection->wire_indices)
      throw std::invalid_argument(
          "Polygon wire topology changed without new geometry");
    auto next = std::make_unique<Stores>();
    next->own_geometry = geometry;
    glGenVertexArrays(1, &next->vao);
    glBindVertexArray(next->vao);
    if (geometry) {
      next->positions =
          upload(GL_ARRAY_BUFFER, bytes(mesh.positions), mesh.positions.data());
      next->normals =
          upload(GL_ARRAY_BUFFER, bytes(mesh.normals), mesh.normals.data());
      next->triangles = upload(GL_ELEMENT_ARRAY_BUFFER, bytes(mesh.indices),
                               mesh.indices.data());
      next->wires = upload(GL_ELEMENT_ARRAY_BUFFER, bytes(display.wire_indices),
                           display.wire_indices.data());
    } else {
      next->positions = stores_->positions;
      next->normals = stores_->normals;
      next->triangles = stores_->triangles;
      next->wires = stores_->wires;
    }
    next->weights = upload(GL_ARRAY_BUFFER, bytes(display.move_weights),
                           display.move_weights.data());
    next->normal_coefficients =
        upload(GL_ARRAY_BUFFER, bytes(display.normal_coefficients),
               display.normal_coefficients.data());
    next->selected_edges =
        upload(GL_ELEMENT_ARRAY_BUFFER, bytes(display.selected_edges),
               display.selected_edges.data());
    next->selected_points =
        upload(GL_ELEMENT_ARRAY_BUFFER, bytes(display.selected_points),
               display.selected_points.data());
    for (auto [attribute, buffer] :
         {std::pair<GLuint, GLuint>{0, next->positions},
          {1, next->normals},
          {2, next->weights},
          {3, next->normal_coefficients}}) {
      glBindBuffer(GL_ARRAY_BUFFER, buffer);
      glVertexAttribPointer(attribute, attribute == 2 ? 1 : 3, GL_FLOAT,
                            GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(attribute);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, next->triangles);
    if (glGetError() != GL_NO_ERROR)
      throw std::runtime_error("Unable to publish polygon GPU attributes");
    next->geometry = view.surface;
    next->selection = view.display;
    if (!geometry) {
      stores_->own_geometry = false;
      next->own_geometry = true;
    }
    stores_ = std::move(next);
    if (geometry)
      ++stats_.geometry_uploads;
    ++stats_.selection_uploads;
    stats_.geometry_bytes = bytes(mesh.positions) + bytes(mesh.normals) +
                            bytes(mesh.indices) + bytes(display.wire_indices);
    stats_.selection_bytes =
        bytes(display.move_weights) + bytes(display.normal_coefficients) +
        bytes(display.selected_edges) + bytes(display.selected_points);
  }
  glEnable(GL_PROGRAM_POINT_SIZE);
  glUseProgram(program_);
  glBindVertexArray(stores_->vao);
  glUniformMatrix4fv(glGetUniformLocation(program_, "object_to_clip"), 1,
                     GL_FALSE, clip.data());
  glUniformMatrix4fv(glGetUniformLocation(program_, "object_to_world"), 1,
                     GL_FALSE, world.data());
  glUniform3f(glGetUniformLocation(program_, "delta"), view.move_delta.x,
              view.move_delta.y, view.move_delta.z);
  glUniform1f(glGetUniformLocation(program_, "point_size"), point_size);
  glUniform1i(glGetUniformLocation(program_, "linear_output"), linear_output);
  glUniform1i(glGetUniformLocation(program_, "moving"),
              view.moving &&
                  (view.move_delta.x != 0 || view.move_delta.y != 0 ||
                   view.move_delta.z != 0));
  glUniform3f(glGetUniformLocation(program_, "accent"),
              linear(float((accent >> 16) & 255) / 255),
              linear(float((accent >> 8) & 255) / 255),
              linear(float(accent & 255) / 255));
  glDisable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1, 1);
  glUniform1i(glGetUniformLocation(program_, "pass"), 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stores_->triangles);
  glDrawElements(GL_TRIANGLES, GLsizei(mesh.indices.size()), GL_UNSIGNED_INT,
                 nullptr);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthMask(GL_FALSE);
  glLineWidth(1);
  glUniform1i(glGetUniformLocation(program_, "pass"), 1);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stores_->wires);
  glDrawElements(GL_LINES, GLsizei(display.wire_indices.size()),
                 GL_UNSIGNED_INT, nullptr);
  glUniform1i(glGetUniformLocation(program_, "pass"), 2);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stores_->selected_edges);
  glDrawElements(GL_LINES, GLsizei(display.selected_edges.size()),
                 GL_UNSIGNED_INT, nullptr);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stores_->selected_points);
  glUseProgram(points_program_);
  glUniformMatrix4fv(glGetUniformLocation(points_program_, "object_to_clip"), 1,
                     GL_FALSE, clip.data());
  glUniform3f(glGetUniformLocation(points_program_, "delta"), view.move_delta.x,
              view.move_delta.y, view.move_delta.z);
  glUniform1f(glGetUniformLocation(points_program_, "point_size"), point_size);
  glUniform1i(glGetUniformLocation(points_program_, "linear_output"),
              linear_output);
  glUniform3f(glGetUniformLocation(points_program_, "accent"),
              linear(float((accent >> 16) & 255) / 255),
              linear(float((accent >> 8) & 255) / 255),
              linear(float(accent & 255) / 255));
  glDrawElements(GL_POINTS, GLsizei(display.selected_points.size()),
                 GL_UNSIGNED_INT, nullptr);
  ++stats_.draws;
}
} // namespace dome
