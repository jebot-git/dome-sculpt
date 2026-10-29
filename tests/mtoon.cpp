// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mtoon.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#ifdef DOME_MTOON_GPU
#define GL_GLEXT_PROTOTYPES
#include "graphics.h"
#include <GL/gl.h>
#include <GL/glext.h>
#endif
using namespace dome;
using Json = nlohmann::json;
void require(bool x, const char *s) {
  if (!x)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << '\n';
}
template <class F> void rejects(F f, const char *s) {
  bool bad = false;
  try {
    f();
  } catch (const std::exception &) {
    bad = true;
  }
  require(bad, s);
}
bool near(float a, float b, float e = 2e-5f) { return std::abs(a - b) < e; }
Json legacy() {
  return {{"shader", "VRM/MToon"},
          {"renderQueue", 2000},
          {"floatProperties",
           {{"_ShadeShift", -.6f}, {"_ShadeToony", 0}, {"_FutureFloat", .125}}},
          {"vectorProperties",
           {{"_Color", {1, 1, 1, 1}},
            {"_ShadeColor", {.97f, .81f, .86f, 1}},
            {"_FutureVector", {1, 2, 3}}}},
          {"textureProperties",
           {{"_MainTex", 0}, {"_ShadeTexture", 0}, {"_BumpMap", 1}}},
          {"keywordMap", {{"_NORMALMAP", true}}},
          {"tagMap", {{"RenderType", "Opaque"}}}};
}
Json modern() {
  return {{"extensions", {{"VRMC_materials_mtoon", {{"specVersion", "1.0"}}}}}};
}
#ifdef DOME_MTOON_GPU
GLuint shader(GLenum type, const std::string &s) {
  GLuint id = glCreateShader(type);
  const char *p = s.c_str();
  glShaderSource(id, 1, &p, nullptr);
  glCompileShader(id);
  GLint ok;
  glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[8192];
    glGetShaderInfoLog(id, sizeof(log), nullptr, log);
    throw std::runtime_error(log);
  }
  return id;
}
void gpu_check() {
  NativeGraphics graphics;
  std::cout << "EGL renderer: " << glGetString(GL_RENDERER) << '\n';
  auto vertex =
      shader(GL_VERTEX_SHADER, "#version 330 core\nvoid main(){vec2 "
                               "p[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));"
                               "gl_Position=vec4(p[gl_VertexID],0,1);}");
  std::string fragment =
      "#version 330 core\n" + std::string(mtoon_glsl()) + R"GLSL(
uniform vec4 u[12];uniform vec3 n,v,l,lc,gi,up,down,shade,emission,rim,matcap;
uniform vec4 base;uniform vec4 scalar;uniform vec4 uvTransform;uniform float uvRotation;uniform sampler2D sourceTexture;uniform int front,outline,checkKind;
out vec4 color;
void main(){DomeMToonSamples s;s.base=base;s.shade=shade;s.emission=emission;s.rim=rim;s.matcap=matcap;s.shift=scalar.x;s.receiveShadow=scalar.y;s.grade=scalar.z;
DomeMToonLight q;q.normal=n;q.view=v;q.direction=l;q.color=lc;q.gi=gi;q.giUp=up;q.giDown=down;q.shadow=scalar.w;q.normalOriented=false;bool killed;
color=dome_mtoon_shade(u,s,q,front!=0,outline!=0,killed);if(killed)discard;
if(checkKind==1)color=dome_mtoon_decode_texel(base,1);
if(checkKind==2)color=vec4(dome_mtoon_normal_map(n,u[6].z),1);
if(checkKind==4)color=vec4(dome_mtoon_shading_normal(int(u[8].x),n,vec4(1,0,0,1),base.rgb,u[6].z,v,front!=0,false),1);
if(checkKind==3)color=vec4(dome_mtoon_uv(u,base.xy,uvTransform.xy,uvTransform.zw,uvRotation,false,false,false,scalar.x,scalar.y),0,1);
if(checkKind==5)color=vec4(dome_mtoon_outline_world_width(u,scalar.z),dome_mtoon_outline_clip_offset(u,base.xy,n,scalar.x,scalar.y,scalar.z),1);
if(checkKind==6)color=texture(sourceTexture,vec2(0.5));
if(checkKind==7)color=vec4(dome_mtoon_material_normal(u,n,vec4(1,0,0,1),base.rgb,v,front!=0,false),1);
if(checkKind==8)color=vec4(dome_mtoon_matcap_uv(n,v,vec3(0,1,0)),0,1);
}
)GLSL";
  auto frag = shader(GL_FRAGMENT_SHADER, fragment);
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex);
  glAttachShader(program, frag);
  glLinkProgram(program);
  GLint ok;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok)
    throw std::runtime_error("MToon test GLSL link failed");
  glUseProgram(program);
  GLuint vao, fbo, texture;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT,
               nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    throw std::runtime_error("MToon float framebuffer unavailable");
  glViewport(0, 0, 1, 1);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_FRAMEBUFFER_SRGB);
  auto set3 = [&](const char *name, Vec3 x) {
    glUniform3f(glGetUniformLocation(program, name), x.x, x.y, x.z);
  };
  auto draw = [&](const MToonMaterial &m, const MToonSamples &s,
                  const MToonLighting &q, int kind = 0) {
    auto u = m.gpu_parameters();
    glUniform4fv(glGetUniformLocation(program, "u[0]"), 12, u[0].data());
    auto &tx = m.textures[0];
    glUniform4f(glGetUniformLocation(program, "uvTransform"), tx.offset[0],
                tx.offset[1], tx.scale[0], tx.scale[1]);
    glUniform1f(glGetUniformLocation(program, "uvRotation"), tx.rotation);
    set3("n", q.normal);
    set3("v", q.view);
    set3("l", q.light_direction);
    set3("lc", q.light_color);
    set3("gi", q.gi);
    set3("up", q.gi_up);
    set3("down", q.gi_down);
    set3("shade", s.shade);
    set3("emission", s.emission);
    set3("rim", s.rim);
    set3("matcap", s.matcap);
    glUniform4fv(glGetUniformLocation(program, "base"), 1, s.base.data());
    glUniform4f(glGetUniformLocation(program, "scalar"), s.shift,
                s.receive_shadow, s.shading_grade, q.shadow);
    glUniform1i(glGetUniformLocation(program, "front"), q.front_facing);
    glUniform1i(glGetUniformLocation(program, "outline"), q.outline_pass);
    glUniform1i(glGetUniformLocation(program, "checkKind"), kind);
    const GLfloat clear[4] = {-7, -7, -7, -7};
    glClearBufferfv(GL_COLOR, 0, clear);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    std::array<float, 4> pixel;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel.data());
    if (glGetError() != GL_NO_ERROR)
      throw std::runtime_error("MToon GPU evaluation error");
    return pixel;
  };
  bool parity = true;
  for (int version = 0; version < 2; version++)
    for (int i = 0; i < 18; i++) {
      auto j = version ? modern() : legacy();
      if (version) {
        j["alphaMode"] = i % 3 == 0 ? "BLEND" : i % 3 == 1 ? "MASK" : "OPAQUE";
        j["pbrMetallicRoughness"]["baseColorFactor"] = {.8f, .6f, .4f, .4f};
      } else {
        j["floatProperties"]["_BlendMode"] = i % 3 == 0   ? 2
                                             : i % 3 == 1 ? 1
                                                          : 0;
        j["vectorProperties"]["_Color"] = {.8f, .6f, .4f, .4f};
      }
      auto m =
          version ? MToonMaterial::from_vrm1(j) : MToonMaterial::from_vrm0(j);
      m.parameters.rim_color = {.1f, .2f, .05f};
      m.parameters.rim_mix = .6f;
      m.parameters.emission = {.02f, .01f, .03f};
      m.parameters.outline_color = {.2f, .1f, .3f};
      m.parameters.toony = float(i % 4) / 3;
      MToonSamples s;
      s.base = {.6f, .7f, .8f, float(i % 4) / 3};
      s.shade = {.3f, .4f, .5f};
      s.rim = {.5f, .6f, .7f};
      s.matcap = {.1f, .05f, .02f};
      s.shift = .05f;
      s.receive_shadow = .8f;
      s.shading_grade = .7f;
      MToonLighting q;
      q.normal = normalized({float(i % 3) - 1, .3f, .7f});
      q.light_direction = normalized({.2f, .4f, 1});
      q.light_color = {.7f, .6f, .9f};
      q.gi = {.03f, .04f, .02f};
      q.gi_up = {.1f, .07f, .02f};
      q.gi_down = {.02f, .03f, .05f};
      q.shadow = .7f;
      q.front_facing = i % 2;
      q.outline_pass = i % 5 == 0;
      auto cpu = mtoon_shade(m, s, q);
      auto pixel = draw(m, s, q);
      for (size_t k = 0; k < 4; k++)
        parity &=
            cpu.discard ? pixel[k] == -7 : near(pixel[k], cpu.color[k], 8e-5f);
    }
  require(parity, "Independent EGL GLSL matches native V0/V1 lighting, alpha "
                  "discard, normal orientation, rim and outline cases");
  auto m = MToonMaterial::from_vrm1(modern());
  MToonSamples s;
  s.base = {.5f, .25f, .75f, .3f};
  MToonLighting q;
  auto pixel = draw(m, s, q, 1);
  auto linear = mtoon_decode_texel(MToonEncoding::Srgb, s.base);
  bool gamma = true;
  for (size_t k = 0; k < 4; k++)
    gamma &= near(pixel[k], linear[k]);
  require(
      gamma,
      "EGL shader decodes sRGB exactly once while retaining linear opacity");
  q.normal = {.5f, 1, 1};
  pixel = draw(m, s, q, 2);
  auto mapped = mtoon_normal_map(q.normal, 1);
  require(near(pixel[0], mapped.x) && near(pixel[1], mapped.y) &&
              near(pixel[2], mapped.z),
          "EGL tangent normal decoding preserves positive green direction");
  s.base = {1, .5f, 1, 1};
  q.normal = {0, 0, 1};
  q.front_facing = false;
  pixel = draw(m, s, q, 4);
  auto back = mtoon_shading_normal(MToonVersion::V1, q.normal, {1, 0, 0, 1},
                                   {1, .5f, 1}, 1, q.view, false);
  require(near(pixel[0], back.x) && near(pixel[2], back.z) && pixel[0] > 0,
          "EGL backface tangent-frame normal mapping keeps positive tangent X "
          "while reversing geometric normal");
  for (int version = 0; version < 2; version++) {
    auto mm = version ? MToonMaterial::from_vrm1(modern())
                      : MToonMaterial::from_vrm0(legacy());
    if (version) {
      mm.textures[0].offset = {.1f, .2f};
      mm.textures[0].scale = {2, 3};
    }
    mm.parameters.scroll = {.25f, 0};
    mm.parameters.rotation_speed = version ? 1.57079632679f : .25f;
    s.base = {1, .5f, 0, 1};
    s.shift = 1;
    s.receive_shadow = 1;
    q.normal = {0, 0, 1};
    auto result = draw(mm, s, q, 3);
    auto uv = mtoon_uv(mm, MToonSlot::Base, {1, .5f}, 1, 1);
    require(
        near(result[0], uv[0]) && near(result[1], uv[1]),
        version
            ? "EGL V1 UV rotation-before-scroll matches glTF-origin convention"
            : "EGL V0 UV scroll-before-rotation retains legacy bottom-left "
              "convention");
  }
  auto mapped_material = modern();
  mapped_material["normalTexture"] = {{"index", 0}};
  auto enabled = MToonMaterial::from_vrm1(mapped_material, 1);
  s.base = {1, .5f, 1, 1};
  q = {};
  q.front_facing = false;
  auto active_normal = draw(enabled, s, q, 7);
  auto disabled_json = legacy();
  disabled_json["keywordMap"]["_NORMALMAP"] = false;
  auto inactive = MToonMaterial::from_vrm0(disabled_json);
  q.front_facing = true;
  auto flat_normal = draw(inactive, s, q, 7);
  require(active_normal[0] > 0 && active_normal[2] < 0 && flat_normal[0] == 0 &&
              flat_normal[2] == 1,
          "EGL material-normal wrapper respects V1 texture activation and "
          "explicit legacy disabled keyword");
  s = {};
  q = {};
  q.normal = {0, 1, 0};
  auto matcap_up = draw(m, s, q, 8);
  q.normal = {1, 0, 0};
  auto matcap_right = draw(m, s, q, 8);
  require(near(matcap_up[1], 0) && near(matcap_right[0], 1),
          "EGL matcap maps right-handed normal-up/right to top/right texture "
          "coordinates");
  bool outline_parity = true;
  for (int v = 0; v < 2; v++)
    for (int mode = 1; mode < 3; mode++) {
      auto mm = v ? MToonMaterial::from_vrm1(modern())
                  : MToonMaterial::from_vrm0(legacy());
      mm.parameters.outline = MToonOutline(mode);
      mm.parameters.outline_width = v ? .01f : 1.f;
      s.base = {1, 0, 0, 1};
      s.shift = 2;
      s.receive_shadow = 2;
      s.shading_grade = .5f;
      q.normal = {1, 0, 0};
      q.front_facing = true;
      auto result = draw(mm, s, q, 5);
      auto clip = mtoon_outline_clip_offset(mm, {1, 0}, q.normal, 2, 2, .5f);
      outline_parity &= near(result[0], mtoon_outline_world_width(mm, .5f)) &&
                        near(result[1], clip[0]) && near(result[2], clip[1]);
    }
  require(outline_parity, "EGL world/screen outline widths retain version "
                          "units, distance fade and aspect correction");
  GLuint srgb;
  glGenTextures(1, &srgb);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, srgb);
  const unsigned char texel[] = {128, 64, 255, 128};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, 1, 1, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, texel);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glUniform1i(glGetUniformLocation(program, "sourceTexture"), 1);
  glActiveTexture(GL_TEXTURE0);
  s = {};
  q = {};
  auto hardware = draw(m, s, q, 6);
  auto expected = mtoon_decode_texel(MToonEncoding::Srgb,
                                     {128.f / 255, 64.f / 255, 1, 128.f / 255});
  bool exact = true;
  float max_rgb_error = 0;
  for (size_t k = 0; k < 4; k++) {
    float error = std::abs(hardware[k] - expected[k]);
    if (k < 3)
      max_rgb_error = std::max(max_rgb_error, error);
    exact &= near(hardware[k], expected[k], k < 3 ? 1e-3f : 2e-5f);
  }
  std::cout << "Hardware sRGB max RGB error=" << max_rgb_error
            << ", alpha error=" << std::abs(hardware[3] - expected[3]) << '\n';
  require(exact, "Actual GPU sRGB storage follows transfer/linear-alpha "
                 "semantics within measured storage precision");
  glDeleteTextures(1, &srgb);
  glDeleteTextures(1, &texture);
  glDeleteFramebuffers(1, &fbo);
  glDeleteVertexArrays(1, &vao);
  glDeleteProgram(program);
  glDeleteShader(vertex);
  glDeleteShader(frag);
}
#endif
int main(int argc, char **argv) try {
  auto source0 = legacy();
  auto zero = MToonMaterial::from_vrm0(source0, 2);
  auto source1 = modern();
  source1["extras"] = {{"original", "keep"}};
  source1["extensions"]["VRMC_materials_mtoon"]["futureField"] = {{"value", 7}};
  auto one = MToonMaterial::from_vrm1(source1);
  require(zero.source_json() == source0 &&
              MToonMaterial::decode(zero.encode(), 2).source_json() == source0,
          "VRM0 property maps and unknown fields round-trip exactly without V1 "
          "conversion");
  require(one.source_json() == source1 &&
              MToonMaterial::decode(one.encode()).source_json() == source1,
          "MToon1 version, omitted fields, extension extras and unmapped "
          "payload round-trip exactly");
  MToonMaterial fresh;
  require(fresh.version() == MToonVersion::V1 &&
              MToonMaterial::decode(fresh.encode()).encode() == fresh.encode(),
          "Default native MToon material is valid and round-trips its explicit "
          "version");
  auto future = one;
  future.parameters.normal_scale = 2;
  future.textures[0].offset = {.2f, .3f};
  future.textures[0].scale = {2, 3};
  auto saved = MToonMaterial::decode(future.encode());
  require(saved.parameters.normal_scale == 2 && saved.textures[0].index == -1 &&
              saved.textures[0].offset == std::array<float, 2>{.2f, .3f} &&
              saved.encode() == future.encode(),
          "Native drafts retain authored settings for unassigned textures "
          "without inventing invalid glTF references");
  auto edit = one;
  edit.parameters.shift = .2f;
  auto edited = edit.source_json();
  require(edited["extensions"]["VRMC_materials_mtoon"]["shadingShiftFactor"] ==
                  .2f &&
              edited["extras"] == source1["extras"] &&
              !edited.contains("pbrMetallicRoughness"),
          "Known edits preserve unknown JSON and do not inject null/default "
          "material fields");
  require(zero.textures[7].channel == 0 && one.textures[7].channel == 1 &&
              zero.textures[8].channel == 0 && one.textures[8].channel == 2,
          "Legacy R masks and VRM1 packed outline-G/animation-B masks remain "
          "distinct");
  require(zero.textures[0].encoding == MToonEncoding::Srgb &&
              zero.textures[2].encoding == MToonEncoding::Normal &&
              one.textures[6].encoding == MToonEncoding::Linear,
          "Texture roles retain sRGB color, linear data and tangent-normal "
          "encodings");
  auto disabled_normal = legacy();
  disabled_normal["keywordMap"]["_NORMALMAP"] = false;
  auto disabled = MToonMaterial::from_vrm0(disabled_normal);
  require(!disabled.parameters.normal_enabled &&
              disabled.textures[2].index == 1 &&
              mtoon_material_normal(disabled, {0, 0, 1}, {1, 0, 0, 1},
                                    {1, .5f, 1}, {0, 0, 1}, true)
                      .x == 0,
          "Legacy normal keyword controls execution independently of retaining "
          "its assigned normal texture");
  require(mtoon_default_texel(MToonSlot::Matcap)[0] == 0 &&
              mtoon_default_texel(MToonSlot::Shift)[0] == 0 &&
              mtoon_default_texel(MToonSlot::UvMask)[2] == 1 &&
              mtoon_default_texel(MToonSlot::Normal)[2] == 1,
          "Absent texture roles expose exact black/white/flat-normal defaults");
  auto transformed = modern();
  transformed["pbrMetallicRoughness"]["baseColorTexture"] = {
      {"index", 0},
      {"extras", {{"asset", "original"}}},
      {"extensions",
       {{"KHR_texture_transform",
         {{"offset", {.1f, .2f}},
          {"rotation", .3f},
          {"texCoord", 1},
          {"extras", {{"keep", true}}}}},
        {"OTHER_texture", {{"payload", 7}}}}}};
  auto texture_edit = MToonMaterial::from_vrm1(transformed, 3);
  texture_edit.textures[0].index = 1;
  auto transformed_out = texture_edit.source_json();
  require(
      transformed_out["pbrMetallicRoughness"]["baseColorTexture"]["index"] ==
              1 &&
          transformed_out["pbrMetallicRoughness"]["baseColorTexture"]
                         ["extensions"] ==
              transformed["pbrMetallicRoughness"]["baseColorTexture"]
                         ["extensions"] &&
          std::any_of(texture_edit.diagnostics.begin(),
                      texture_edit.diagnostics.end(),
                      [](const std::string &s) {
                        return s.find("OTHER_texture") != std::string::npos;
                      }),
      "Texture index edits preserve KHR transforms, nested unknown payload and "
      "named unsupported-extension diagnostics");
  auto gamma0 = source0;
  gamma0["vectorProperties"]["_Color"] = {.5f, .5f, .5f, .5f};
  auto g0 = MToonMaterial::from_vrm0(gamma0);
  auto gamma1 = modern();
  gamma1["pbrMetallicRoughness"]["baseColorFactor"] = {.5f, .5f, .5f, .5f};
  auto g1 = MToonMaterial::from_vrm1(gamma1);
  require(near(g0.parameters.base_color[0], .21404114f) &&
              g1.parameters.base_color[0] == .5f &&
              g0.parameters.base_color[3] == .5f,
          "V0 gamma color factors and V1 linear factors preserve different "
          "semantics and linear alpha");
  auto blend0 = legacy();
  blend0["floatProperties"]["_BlendMode"] = 3;
  blend0["floatProperties"]["_ZWrite"] = 1;
  auto state0 = MToonMaterial::from_vrm0(blend0).render_state();
  auto blend1 = modern();
  blend1["alphaMode"] = "BLEND";
  blend1["extensions"]["VRMC_materials_mtoon"]["transparentWithZWrite"] = true;
  blend1["extensions"]["VRMC_materials_mtoon"]["renderQueueOffsetNumber"] = 5;
  auto state1 = MToonMaterial::from_vrm1(blend1).render_state(true);
  require(state0.depth_write && state0.maximum_alpha &&
              state0.source_alpha == 5 && state1.depth_write &&
              !state1.maximum_alpha && state1.source_alpha == 1 &&
              state1.queue == 2506 && state1.cull == MToonCull::Front,
          "Render state preserves legacy alpha-Max blending and VRM1 "
          "depth/queue/outline-front-cull semantics");
  auto sample = mtoon_decode_texel(MToonEncoding::Srgb, {.5f, .25f, 1, .5f});
  require(near(sample[0], .21404114f) && sample[3] == .5f,
          "sRGB texel decoding leaves opacity linear");
  auto simple = modern();
  simple["extensions"]["VRMC_materials_mtoon"]["shadeColorFactor"] = {0, 0, 0};
  auto toon = MToonMaterial::from_vrm1(simple);
  MToonSamples samples;
  MToonLighting light;
  auto lit = mtoon_shade(toon, samples, light);
  light.light_direction = {0, 0, -1};
  auto shade = mtoon_shade(toon, samples, light);
  require(lit.color[0] == 1 && shade.color[0] == 0,
          "VRM1 direct lighting reaches independent lit and shade endpoints");
  light.light_direction = {1, 0, 0};
  toon.parameters.toony = 0;
  auto half = mtoon_shade(toon, samples, light);
  require(near(half.color[0], .5f),
          "VRM1 zero-toony midpoint follows symmetric Lambert boundary");
  toon.parameters.toony = 1;
  auto sharp = mtoon_shade(toon, samples, light);
  require(std::isfinite(sharp.color[0]) && sharp.color[0] == 1,
          "Fully sharp VRM1 transition has deterministic finite threshold "
          "behavior");
  auto a0 = zero;
  a0.parameters.base_color = {1, 1, 1, 1};
  a0.parameters.shade_color = {0, 0, 0};
  a0.parameters.toony = 0;
  a0.parameters.shift = -.6f;
  auto a1 = toon;
  a1.parameters.toony = 0;
  a1.parameters.shift = -.6f;
  auto legacy_mid = mtoon_shade(a0, samples, light),
       modern_mid = mtoon_shade(a1, samples, light);
  require(near(legacy_mid.color[0], .375f) && near(modern_mid.color[0], .2f),
          "Legacy asymmetric shift/toony curve differs from VRM1 symmetric "
          "additive shift");
  auto alpha = modern();
  alpha["alphaMode"] = "BLEND";
  alpha["pbrMetallicRoughness"]["baseColorFactor"] = {1, 1, 1, .5f};
  auto transparent = MToonMaterial::from_vrm1(alpha);
  samples.base[3] = .5f;
  require(mtoon_shade(transparent, samples, light).color[3] == .25f &&
              !transparent.parameters.depth_write,
          "Straight transparent opacity multiplies factor/texture and disables "
          "depth writes by default");
  alpha["alphaMode"] = "MASK";
  auto mask = MToonMaterial::from_vrm1(alpha);
  require(mtoon_shade(mask, samples, light).discard,
          "Mask opacity below cutoff discards the fragment");
  samples.base[3] = 1;
  require(!mtoon_shade(mask, samples, light).discard &&
              mtoon_shade(mask, samples, light).color[3] == 1,
          "Mask cutoff equality survives with full opacity");
  alpha["alphaMode"] = "OPAQUE";
  alpha["pbrMetallicRoughness"]["baseColorFactor"][3] = 0;
  require(
      mtoon_shade(MToonMaterial::from_vrm1(alpha), samples, light).color[3] ==
          1,
      "Opaque mode ignores authored opacity without premultiplying RGB");
  light = {};
  light.light_color = {0, 0, 0};
  light.gi = {.1f, .2f, .3f};
  light.gi_up = {.8f, .6f, .4f};
  light.gi_down = {.4f, .2f, 0};
  auto gi = one;
  gi.parameters.gi_equalization = 0;
  auto raw = mtoon_shade(gi, samples, light);
  gi.parameters.gi_equalization = 1;
  auto equal = mtoon_shade(gi, samples, light);
  require(near(raw.color[0], .1f) && near(equal.color[0], .6f) &&
              near(equal.color[2], .2f),
          "VRM1 GI equalization interpolates directional irradiance toward "
          "up/down average");
  light = {};
  light.light_color = {0, 0, 0};
  auto glow = one;
  glow.parameters.emission = {.5f, 0, 0};
  glow.parameters.emission_strength = 2;
  require(mtoon_shade(glow, samples, light).color[0] == 1,
          "Linear emission and explicit emissive strength add independently of "
          "lighting");
  auto rim = one;
  rim.parameters.base_color = {0, 0, 0, 1};
  rim.parameters.rim_color = {.2f, 0, 0};
  rim.parameters.matcap_color = {.5f, 0, 0};
  rim.parameters.rim_mix = 0;
  light.normal = {1, 0, 0};
  samples.matcap = {.4f, 0, 0};
  samples.rim = {.5f, 1, 1};
  require(
      near(mtoon_shade(rim, samples, light).color[0], .2f),
      "VRM1 rim texture multiplies both parametric rim and additive matcap");
  auto rim0 = zero;
  rim0.parameters.base_color = {0, 0, 0, 1};
  rim0.parameters.rim_color = {.2f, 0, 0};
  rim0.parameters.rim_mix = 0;
  require(near(mtoon_shade(rim0, samples, light).color[0], .5f),
          "Legacy matcap stays additive outside parametric rim texture "
          "multiplication");
  auto outline = one;
  outline.parameters.outline_color = {.2f, 0, 0};
  outline.parameters.emission = {.4f, 0, 0};
  light = {};
  light.light_color = {0, 0, 0};
  light.outline_pass = true;
  light.front_facing = false;
  samples = {};
  require(
      near(mtoon_shade(outline, samples, light).color[0], .08f),
      "VRM1 outline lighting includes the final emission/rim surface result");
  outline = zero;
  outline.parameters.outline_color = {.2f, 0, 0};
  outline.parameters.emission = {.4f, 0, 0};
  require(mtoon_shade(outline, samples, light).color[0] == 0,
          "Legacy outline omits emission, rim and matcap before outline "
          "multiplication");
  require(
      mtoon_facing_normal(MToonVersion::V0, {0, 0, -1}, {0, 0, 1}, true).z ==
              1 &&
          mtoon_facing_normal(MToonVersion::V1, {0, 0, -1}, {0, 0, 1}, true)
                  .z == -1 &&
          mtoon_facing_normal(MToonVersion::V1, {0, 0, -1}, {0, 0, 1}, false)
                  .z == 1,
      "Legacy view-facing normal flips remain distinct from glTF front-face "
      "orientation");
  auto normal = mtoon_normal_map({.5f, 1, 1}, 1);
  require(normal.y > 0 && mtoon_normal_map({.5f, 1, 1}, -1).y < 0 &&
              mtoon_normal_map({.5f, .5f, .5f}, 1).z == 1,
          "Normal-map green direction and signed strength remain explicit and "
          "finite");
  auto mapped_front =
      mtoon_shading_normal(MToonVersion::V1, {0, 0, 1}, {1, 0, 0, 1},
                           {1, .5f, 1}, 1, {0, 0, 1}, true);
  auto mapped_back =
      mtoon_shading_normal(MToonVersion::V1, {0, 0, 1}, {1, 0, 0, 1},
                           {1, .5f, 1}, 1, {0, 0, 1}, false);
  require(mapped_front.x > 0 && mapped_back.x > 0 && mapped_front.z > 0 &&
              mapped_back.z < 0,
          "Backface normal mapping reverses geometric normal before TBN "
          "without incorrectly reversing tangent X");
  auto uv1 = one;
  uv1.parameters.scroll = {.25f, 0};
  uv1.parameters.rotation_speed = 1.57079632679f;
  auto uv = mtoon_uv(uv1, MToonSlot::Base, {1, .5f}, 1);
  require(
      near(uv[0], .75f) && near(uv[1], 0),
      "VRM1 rotates before independent scrolling in glTF top-left coordinates");
  auto uv0 = zero;
  uv0.parameters.scroll = {.25f, 0};
  uv0.parameters.rotation_speed = .25f;
  uv = mtoon_uv(uv0, MToonSlot::Base, {1, .5f}, 1);
  require(near(uv[0], .5f) && near(uv[1], -.25f),
          "VRM0 retains turns per second and scroll-before-rotation in "
          "bottom-left coordinates");
  uv1.textures[0].offset = {.1f, .2f};
  uv1.textures[0].scale = {2, 3};
  uv = mtoon_uv(uv1, MToonSlot::Base, {1, .5f}, 1);
  require(near(uv[0], 1.6f) && near(uv[1], .2f),
          "KHR texture transform runs after MToon1 animation");
  uv = mtoon_uv(uv1, MToonSlot::Base, {1, .5f}, 1, 0);
  require(near(uv[0], 2.1f) && near(uv[1], 1.7f),
          "Zero UV animation mask leaves only authored texture transform");
  auto width0 = zero;
  width0.parameters.outline = MToonOutline::World;
  width0.parameters.outline_width = 2;
  auto width1 = one;
  width1.parameters.outline = MToonOutline::World;
  width1.parameters.outline_width = .02f;
  require(
      near(mtoon_outline_world_width(width0), .02f) &&
          near(mtoon_outline_world_width(width1), .02f),
      "Legacy centimetre-scaled widths stay distinct from VRM1 metre widths");
  width1.parameters.outline = MToonOutline::Screen;
  width1.parameters.outline_width = .01f;
  auto clip = mtoon_outline_clip_offset(width1, {1, 0}, {1, 0, 0}, 2, 2);
  require(near(clip[0], .02f), "VRM1 screen outlines use screen-height ratio "
                               "and aspect-correct clip displacement");
  auto matcap_up = mtoon_matcap_uv({0, 1, 0}, {0, 0, 1});
  auto matcap_right = mtoon_matcap_uv({1, 0, 0}, {0, 0, 1});
  require(near(matcap_up[1], 0) && near(matcap_right[0], 1),
          "Matcap coordinates preserve right-handed camera axes and top-left "
          "image orientation");
  auto pole = mtoon_matcap_uv({0, 1, 0}, {0, 1, 0});
  require(std::isfinite(pole[0]) && std::isfinite(pole[1]),
          "Matcap frame remains finite when view and world-up align");
  auto native_bad = one.encode();
  native_bad["version"] = 1.5;
  rejects([&] { MToonMaterial::decode(native_bad); },
          "Native material schema versions reject fractional values without "
          "coercion");
  auto invalid = modern();
  invalid["extensions"]["VRMC_materials_mtoon"]["specVersion"] = "2.0";
  rejects([&] { MToonMaterial::from_vrm1(invalid); },
          "Unknown MToon schema versions reject rather than silently coerce");
  invalid = modern();
  invalid["extensions"]["VRMC_materials_mtoon"]["shadingToonyFactor"] = 1.1;
  rejects([&] { MToonMaterial::from_vrm1(invalid); },
          "MToon parameter ranges reject malformed values");
  invalid = modern();
  invalid["alphaMode"] = "BLEND";
  invalid["extensions"]["VRMC_materials_mtoon"]["renderQueueOffsetNumber"] = 1;
  rejects([&] { MToonMaterial::from_vrm1(invalid); },
          "Transparent queue offsets respect depth-write partition semantics");
  invalid = modern();
  invalid["normalTexture"] = {{"index", 2}};
  rejects([&] { MToonMaterial::from_vrm1(invalid, 2); },
          "Texture indices validate against the owning source asset");
  invalid = modern();
  invalid["pbrMetallicRoughness"]["baseColorFactor"] = {1, 1};
  rejects([&] { MToonMaterial::from_vrm1(invalid); },
          "Malformed color array dimensions cannot truncate silently");
  invalid = modern();
  invalid["extensions"]["VRMC_materials_mtoon"]["shadingShiftFactor"] =
      std::numeric_limits<double>::quiet_NaN();
  rejects([&] { MToonMaterial::from_vrm1(invalid); },
          "Nonfinite raw and typed MToon values reject");
  invalid = legacy();
  invalid["floatProperties"]["_CullMode"] = 2.00000001;
  rejects([&] { MToonMaterial::from_vrm0(invalid); },
          "Legacy enumerations require exact integer values without float "
          "rounding");
  for (int i = 1; i < argc; i++) {
    std::ifstream file(argv[i], std::ios::binary | std::ios::ate);
    auto size = file.tellg();
    if (size < 20 || size > 64 * 1024 * 1024)
      throw std::runtime_error("Invalid VRM material reference fixture size");
    std::vector<uint8_t> bytes(size);
    file.seekg(0);
    file.read(reinterpret_cast<char *>(bytes.data()), size);
    uint32_t length = uint32_t(bytes[12]) | (uint32_t(bytes[13]) << 8) |
                      (uint32_t(bytes[14]) << 16) | (uint32_t(bytes[15]) << 24);
    if (length > bytes.size() - 20)
      throw std::runtime_error("Malformed fixture JSON chunk");
    auto j = Json::parse(bytes.begin() + 20, bytes.begin() + 20 + length);
    size_t parsed = 0;
    for (auto &m : j["extensions"]["VRM"]["materialProperties"]) {
      auto material = MToonMaterial::from_vrm0(m, j["textures"].size());
      if (material.source_json() != m)
        throw std::runtime_error(
            "Original VRoid material changed on roundtrip");
      ++parsed;
    }
    require(parsed == 15, "All15 actual verified beta VRoid MToon0 materials "
                          "preserve exact source maps and version semantics");
  }
#ifdef DOME_MTOON_GPU
  gpu_check();
#endif
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
