// SPDX-License-Identifier: GPL-3.0-or-later
// Independent VRM1 sample consumer: the shader below implements the pinned
// MToon1 direct/GI and UV-animation equations, without production MToon
// helpers.
#define main original_material_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/import-authoring.hpp"
#include "dome/vrm.hpp"
#include "vrm-role-consumer.hpp"
#include <unistd.h>
namespace {
using J = nlohmann::json;
struct V1Consumer {
  GLuint program = 0, vao = 0, vbo = 0, ibo = 0;
  V1Consumer() {
    const char *vs = R"GLSL(#version 330 core
layout(location=0)in vec3 p;layout(location=1)in vec3 N;
layout(location=2)in vec2 UV;layout(location=3)in vec2 SU;
out vec3 n;out vec2 uv,shadeUV;
void main(){n=N;uv=UV;shadeUV=SU;gl_Position=vec4(p,1);})GLSL";
    const char *fs = R"GLSL(#version 330 core
in vec3 n;in vec2 uv,shadeUV;out vec4 result;
uniform sampler2D baseMap,shadeMap;
uniform vec4 baseFactor;uniform vec3 shadeFactor;
uniform vec2 scroll;uniform float speed,seconds,shift,toony,equalization;
uniform vec3 baseX,baseY,shadeX,shadeY;
vec2 animated(vec2 u){float a=speed*seconds;vec2 q=u-.5;
return vec2(cos(a)*q.x+sin(a)*q.y,-sin(a)*q.x+cos(a)*q.y)+.5+scroll*seconds;}
vec2 mapped(vec2 u,vec3 X,vec3 Y){vec3 q=vec3(animated(u),1);return vec2(dot(X,q),dot(Y,q));}
void main(){vec3 N=normalize(n);if(!gl_FrontFacing)N=-N;
vec4 lit=texture(baseMap,mapped(uv,baseX,baseY))*baseFactor;
vec3 shade=texture(shadeMap,mapped(shadeUV,shadeX,shadeY)).rgb*shadeFactor;
float width=1-toony,x=dot(N,normalize(vec3(-.4,.8,.7)))+shift;
float ratio=width<=.0000001?step(0,x):clamp((x+width)/(2*width),0,1);
float gi=mix(mix(.1,.25,.5+.5*N.y),.175,equalization);
result=vec4(mix(shade,lit.rgb,ratio)+gi*lit.rgb,1);})GLSL";
    auto v = role_consumer::shader(GL_VERTEX_SHADER, vs);
    auto f = role_consumer::shader(GL_FRAGMENT_SHADER, fs);
    program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
      throw std::runtime_error("Independent V1 shader link failed");
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
  }
  ~V1Consumer() {
    glDeleteBuffers(1, &ibo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
  }
  GLint loc(const char *s) { return glGetUniformLocation(program, s); }
  void scalar(const char *name, float value) { glUniform1f(loc(name), value); }
  std::array<float, 2> coordinates(const cgltf_primitive &p, const J &info,
                                   size_t v) {
    int channel = info.value("texCoord", 0);
    if (info.contains("extensions") &&
        info.at("extensions").contains("KHR_texture_transform"))
      channel = info.at("extensions")
                    .at("KHR_texture_transform")
                    .value("texCoord", channel);
    auto accessor = role_consumer::attr(p.attributes, p.attributes_count,
                                        cgltf_attribute_type_texcoord, channel);
    std::array<float, 2> uv{};
    if (!accessor || !cgltf_accessor_read_float(accessor, v, uv.data(), 2))
      throw std::runtime_error("Independent V1 UV accessor missing");
    return uv;
  }
  GLuint texture(cgltf_data *data, const J &info, unsigned unit,
                 const char *uniform) {
    int index = info.at("index").get<int>();
    if (index < 0 || size_t(index) >= data->textures_count)
      throw std::runtime_error("V1 texture index invalid");
    auto &t = data->textures[index];
    auto image = role_consumer::image(&t);
    GLuint id;
    glGenTextures(1, &id);
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindSampler(unit, 0);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, image.width, image.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    t.sampler ? t.sampler->wrap_s : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    t.sampler ? t.sampler->wrap_t : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    t.sampler && t.sampler->min_filter ? t.sampler->min_filter
                                                       : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    t.sampler && t.sampler->mag_filter ? t.sampler->mag_filter
                                                       : GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glUniform1i(loc(uniform), unit);
    return id;
  }
  void transform(const J &info, const char *x, const char *y) {
    J t = J::object();
    if (info.contains("extensions") &&
        info.at("extensions").contains("KHR_texture_transform"))
      t = info.at("extensions").at("KHR_texture_transform");
    auto offset = t.value("offset", std::array<float, 2>{0, 0}),
         scale = t.value("scale", std::array<float, 2>{1, 1});
    double a = t.value("rotation", 0.0);
    glUniform3f(loc(x), float(std::cos(a) * scale[0]),
                float(-std::sin(a) * scale[1]), offset[0]);
    glUniform3f(loc(y), float(std::sin(a) * scale[0]),
                float(std::cos(a) * scale[1]), offset[1]);
  }
  Pixels render(role_consumer::Model &model, double seconds,
                role_consumer::Matrix prefix) {
    glViewport(0, 0, 96, 96);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    for (size_t ni = 0; ni < model.data->nodes_count; ++ni) {
      auto &node = model.data->nodes[ni];
      if (!node.mesh)
        continue;
      if (node.skin)
        throw std::runtime_error(
            "This independent official UV-animation oracle is static-only");
      role_consumer::Matrix W{};
      cgltf_node_transform_world(&node, W.data());
      W = role_consumer::multiply(prefix, W);
      auto NW = role_consumer::normal_matrix(W);
      for (size_t pi = 0; pi < node.mesh->primitives_count; ++pi) {
        auto &p = node.mesh->primitives[pi];
        if (!p.material)
          throw std::runtime_error("V1 oracle requires authored material");
        auto &m = model.json.at("materials")
                      .at(size_t(p.material - model.data->materials));
        auto &ext = m.at("extensions").at("VRMC_materials_mtoon");
        if (m.value("alphaMode", std::string("OPAQUE")) != "OPAQUE" ||
            !m.value("doubleSided", false) || p.targets_count)
          throw std::runtime_error(
              "Official animation oracle requires its actual opaque "
              "double-sided static primitives");
        for (const char *role :
             {"normalTexture", "emissiveTexture", "occlusionTexture"})
          if (m.contains(role))
            throw std::runtime_error("Unexpected independent V1 role");
        for (const char *role :
             {"matcapTexture", "rimMultiplyTexture",
              "outlineWidthMultiplyTexture", "shadingShiftTexture",
              "uvAnimationMaskTexture"})
          if (ext.contains(role))
            throw std::runtime_error("Unexpected independent V1 MToon role");
        auto &base = m.at("pbrMetallicRoughness");
        auto &bi = base.at("baseColorTexture");
        auto &si = ext.at("shadeMultiplyTexture");
        auto baseColor =
            base.value("baseColorFactor", std::array<float, 4>{1, 1, 1, 1});
        auto shadeColor =
            ext.value("shadeColorFactor", std::array<float, 3>{0, 0, 0});
        glUniform4fv(loc("baseFactor"), 1, baseColor.data());
        glUniform3fv(loc("shadeFactor"), 1, shadeColor.data());
        scalar("seconds", float(seconds));
        scalar("speed", ext.value("uvAnimationRotationSpeedFactor", 0.f));
        glUniform2f(loc("scroll"),
                    ext.value("uvAnimationScrollXSpeedFactor", 0.f),
                    ext.value("uvAnimationScrollYSpeedFactor", 0.f));
        scalar("shift", ext.value("shadingShiftFactor", 0.f));
        scalar("toony", ext.value("shadingToonyFactor", .9f));
        scalar("equalization", ext.value("giEqualizationFactor", .9f));
        transform(bi, "baseX", "baseY");
        transform(si, "shadeX", "shadeY");
        auto bt = texture(model.data, bi, 0, "baseMap"),
             st = texture(model.data, si, 1, "shadeMap");
        auto P = role_consumer::attr(p.attributes, p.attributes_count,
                                     cgltf_attribute_type_position);
        auto N = role_consumer::attr(p.attributes, p.attributes_count,
                                     cgltf_attribute_type_normal);
        if (!P || !N)
          throw std::runtime_error("V1 authored geometry missing");
        struct Vertex {
          Vec3 p, n;
          std::array<float, 2> uv, shade;
        };
        std::vector<Vertex> vertices;
        for (size_t v = 0; v < P->count; ++v)
          vertices.push_back(
              {role_consumer::point(W, role_consumer::vec(P, v)),
               role_consumer::direction(NW, role_consumer::vec(N, v)),
               coordinates(p, bi, v), coordinates(p, si, v)});
        std::vector<uint32_t> indices;
        for (size_t c = 0; c < (p.indices ? p.indices->count : P->count); ++c)
          indices.push_back(p.indices ? cgltf_accessor_read_index(p.indices, c)
                                      : c);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                     vertices.data(), GL_STREAM_DRAW);
        for (GLuint a = 0; a < 4; ++a) {
          glVertexAttribPointer(
              a, a < 2 ? 3 : 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
              reinterpret_cast<void *>(a == 0   ? offsetof(Vertex, p)
                                       : a == 1 ? offsetof(Vertex, n)
                                       : a == 2 ? offsetof(Vertex, uv)
                                                : offsetof(Vertex, shade)));
          glEnableVertexAttribArray(a);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t),
                     indices.data(), GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES, GLsizei(indices.size()), GL_UNSIGNED_INT,
                       nullptr);
        glDeleteTextures(1, &bt);
        glDeleteTextures(1, &st);
      }
    }
    return read();
  }
};
} // namespace
int main(int argc, char **argv) try {
  if (argc < 3)
    return 2;
  std::filesystem::path out = argv[2];
  std::filesystem::create_directories(out);
  auto home = out / "home";
  std::filesystem::create_directories(home);
  setenv("HOME", home.c_str(), 1);
  setenv("XDG_CONFIG_HOME", home.c_str(), 1);
  setenv("XDG_STATE_HOME", home.c_str(), 1);
  NativeGraphics graphics;
  std::string vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR)),
              device = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  std::cout << "GL_VENDOR " << vendor << "\nGL_RENDERER " << device
            << std::endl;
  check(argc < 4 || std::string_view(argv[3]) != "--hardware" ||
            (vendor.find("Intel") != std::string::npos &&
             device.find("Arc") != std::string::npos &&
             device.find("A770") != std::string::npos &&
             device.find("llvmpipe") == std::string::npos &&
             device.find("UHD") == std::string::npos),
        "Strict explicit hardware request identifies actual Intel Arc A770");
  ImportAuthoringOptions options;
  options.paint_resolution = 64;
  auto prepared = prepare_authoring_import(Scene{}, argv[1], options);
  Document doc;
  doc.replace(prepared.scene);
  check(doc.wait_mesh(std::chrono::seconds(60)) &&
            doc.frame().state->error.empty(),
        "Official VRM1 reaches actual native immutable renderer");
  auto snap = doc.frame().surface;
  Vec3 lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
  for (auto p : snap->mesh.positions) {
    lo.x = std::min(lo.x, p.x);
    lo.y = std::min(lo.y, p.y);
    lo.z = std::min(lo.z, p.z);
    hi.x = std::max(hi.x, p.x);
    hi.y = std::max(hi.y, p.y);
    hi.z = std::max(hi.z, p.z);
  }
  auto center = (lo + hi) * .5f;
  float scale = 1.6f / std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z});
  CanvasTransform fit;
  fit.scale = scale;
  fit.position = {-center.x * scale, -center.y * scale, -center.z * scale};
  auto scene = prepared.scene;
  std::vector<uint64_t> roots;
  for (auto &g : scene.groups())
    if (!g.parent)
      roots.push_back(g.id);
  auto root = scene.add_group("Independent test camera placement", fit);
  for (auto id : roots)
    scene.group(id).parent = root;
  doc.replace(scene);
  check(doc.wait_mesh(std::chrono::seconds(60)) &&
            doc.frame().state->error.empty(),
        "Actual grouped source graph fits camera without flattening geometry");
  auto path = out / "official-animation.vrm";
  export_vrm(scene.preview_surfaces(), path);
  role_consumer::Model source(argv[1]), exported(path);
  role_consumer::Matrix prefix{scale,
                               0,
                               0,
                               0,
                               0,
                               scale,
                               0,
                               0,
                               0,
                               0,
                               scale,
                               0,
                               fit.position.x,
                               fit.position.y,
                               fit.position.z,
                               1};
  Fbo framebuffer;
  SurfaceRenderer native;
  native.set_output_encoding(SurfaceOutputEncoding::Linear);
  V1Consumer consumer;
  Pixels previous;
  for (double seconds : {0., .37, 1.1}) {
    native.set_frame_time(seconds);
    framebuffer.bind();
    auto actual = draw(native, doc.frame().surface);
    framebuffer.bind();
    auto original = consumer.render(source, seconds, prefix);
    framebuffer.bind();
    auto current = consumer.render(exported, seconds, identity);
    std::cout << "VRM1_PIXEL time=" << seconds
              << " original=" << delta(actual, original)
              << " current=" << delta(actual, current) << std::endl;
    check(delta(actual, original) <= 2,
          "Actual native VRM1 agrees with independently parsed unmodified "
          "official source and normative GLSL at tolerance2");
    check(delta(actual, current) <= 2,
          "Current source-node VRM1 export agrees with independent parsed-file "
          "consumer at tolerance2");
    size_t visible = 0;
    for (size_t p = 3; p < actual.size(); p += 4)
      visible += actual[p] > 0;
    check(visible > 500,
          "Full image oracle includes visible official sample pixels");
    if (!previous.empty())
      check(delta(actual, previous) > 16,
            "Authored scrolling/rotation produces a visible independent time "
            "sensitivity");
    previous = actual;
  }
  doc.shading(false);
  check(doc.wait_mesh(std::chrono::seconds(60)) &&
            doc.frame().state->error.empty(),
        "Actual public PBR preview command publishes its immutable role");
  native.set_frame_time(1.1);
  framebuffer.bind();
  check(delta(draw(native, doc.frame().surface), previous) > 16,
        "Real PBR substitution cannot satisfy authored VRM1 pixel oracle");
  std::cout << checks << " official VRM1 independent image checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
