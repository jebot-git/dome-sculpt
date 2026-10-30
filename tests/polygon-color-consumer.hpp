// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Independent unlit current-COLOR_0 consumer. Tangents are optional in glTF
// and irrelevant to this deliberately unlit test; the separate frame consumer
// retains its strict direction-channel requirements unchanged.
#include "source-color-consumer.hpp"
namespace polygon_color_consumer {
using namespace dome;
struct Renderer {
  GLuint program = 0, resolve = 0, vao = 0, vbo = 0, ibo = 0;
  Renderer() {
    auto link = [](const char *v, const char *f) {
      auto vs = color_consumer::shader(GL_VERTEX_SHADER, v),
           fs = color_consumer::shader(GL_FRAGMENT_SHADER, f);
      auto p = glCreateProgram();
      glAttachShader(p, vs);
      glAttachShader(p, fs);
      glLinkProgram(p);
      glDeleteShader(vs);
      glDeleteShader(fs);
      GLint ok;
      glGetProgramiv(p, GL_LINK_STATUS, &ok);
      if (!ok)
        throw std::runtime_error(
            "Independent current-color consumer link failed");
      return p;
    };
    program = link(R"GLSL(#version 330 core
layout(location=0)in vec3 p;layout(location=1)in vec4 c;out vec4 color;
void main(){color=c;gl_Position=vec4(p,1);})GLSL",
                   R"GLSL(#version 330 core
in vec4 color;out vec4 result;uniform vec4 factor;uniform int mode;uniform float cutoff;
void main(){vec4 value=color*factor;if(mode==0)value.a=1;else if(mode==1){if(value.a<cutoff)discard;value.a=1;}if(value.a<=0)discard;result=value;})GLSL");
    resolve = link(R"GLSL(#version 330 core
out vec2 uv;void main(){uv=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(uv*2-1,0,1);})GLSL",
                   R"GLSL(#version 330 core
in vec2 uv;out vec4 color;uniform sampler2D scene;
void main(){vec4 c=texture(scene,uv);vec3 rgb=mix(1.055*pow(max(c.rgb,vec3(0)),vec3(1./2.4))-.055,12.92*c.rgb,lessThanEqual(c.rgb,vec3(.0031308)));color=vec4(rgb,c.a);})GLSL");
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
  }
  ~Renderer() {
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    glDeleteProgram(resolve);
  }
  void render(cgltf_data *data) {
    GLint destination;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &destination);
    struct Linear {
      GLuint f = 0, t = 0, d = 0;
      Linear() {
        glGenFramebuffers(1, &f);
        glBindFramebuffer(GL_FRAMEBUFFER, f);
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 96, 96, 0, GL_RGBA,
                     GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, t, 0);
        glGenRenderbuffers(1, &d);
        glBindRenderbuffer(GL_RENDERBUFFER, d);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 96, 96);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, d);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
          throw std::runtime_error("Independent color target failed");
      }
      ~Linear() {
        glDeleteRenderbuffers(1, &d);
        glDeleteTextures(1, &t);
        glDeleteFramebuffers(1, &f);
      }
    } linear;
    glViewport(0, 0, 96, 96);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    for (size_t ni = 0; ni < data->nodes_count; ++ni) {
      auto &node = data->nodes[ni];
      if (!node.mesh)
        continue;
      std::array<float, 16> world{};
      cgltf_node_transform_world(&node, world.data());
      for (size_t pi = 0; pi < node.mesh->primitives_count; ++pi) {
        auto &primitive = node.mesh->primitives[pi];
        auto position = color_consumer::attr(primitive.attributes,
                                             primitive.attributes_count,
                                             cgltf_attribute_type_position),
             colors = color_consumer::attr(primitive.attributes,
                                           primitive.attributes_count,
                                           cgltf_attribute_type_color);
        if (!position || !colors || !primitive.material ||
            !primitive.material->unlit || position->count != colors->count)
          throw std::runtime_error("Independent current-color consumer "
                                   "requires unlit position/COLOR_0 authority");
        const auto &material = *primitive.material;
        if (material.has_pbr_metallic_roughness &&
            material.pbr_metallic_roughness.base_color_texture.texture) {
          auto image = color_consumer::image(
              material.pbr_metallic_roughness.base_color_texture.texture);
          if (!std::all_of(image.rgba.begin(), image.rgba.end(),
                           [](auto v) { return v == 255; }))
            throw std::runtime_error(
                "Current-color fixture consumer only accepts independently "
                "verified white base texture");
        }
        struct Vertex {
          Vec3 p;
          std::array<float, 4> c;
        };
        std::vector<Vertex> vertices;
        for (size_t i = 0; i < position->count; ++i) {
          auto p = color_consumer::vec(position, i);
          for (size_t target = 0; target < primitive.targets_count; ++target) {
            float weight =
                node.weights_count
                    ? (target < node.weights_count ? node.weights[target] : 0)
                    : (target < node.mesh->weights_count
                           ? node.mesh->weights[target]
                           : 0);
            if (auto a = color_consumer::attr(
                    primitive.targets[target].attributes,
                    primitive.targets[target].attributes_count,
                    cgltf_attribute_type_position))
              p += color_consumer::vec(a, i) * weight;
          }
          if (node.skin) {
            Vec3 posed{};
            for (int set = 0;; ++set) {
              auto joints = color_consumer::attr(
                       primitive.attributes, primitive.attributes_count,
                       cgltf_attribute_type_joints, set),
                   weights = color_consumer::attr(
                       primitive.attributes, primitive.attributes_count,
                       cgltf_attribute_type_weights, set);
              if (!joints)
                break;
              float ji[4]{}, w[4]{};
              if (!weights || !cgltf_accessor_read_float(joints, i, ji, 4) ||
                  !cgltf_accessor_read_float(weights, i, w, 4))
                throw std::runtime_error(
                    "Current-color consumer skin data missing");
              for (size_t k = 0; k < 4; ++k)
                if (w[k] != 0) {
                  auto index = size_t(ji[k]);
                  if (index >= node.skin->joints_count)
                    throw std::runtime_error(
                        "Current-color joint index invalid");
                  color_consumer::Matrix joint{},
                      bind{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                  cgltf_node_transform_world(node.skin->joints[index],
                                             joint.data());
                  if (node.skin->inverse_bind_matrices &&
                      !cgltf_accessor_read_float(
                          node.skin->inverse_bind_matrices, index, bind.data(),
                          16))
                    throw std::runtime_error("Current-color IBM missing");
                  posed += color_consumer::point(
                               color_consumer::multiply(joint, bind), p) *
                           w[k];
                }
            }
            p = posed;
          } else
            p = color_consumer::point(world, p);
          float c[4]{};
          if (!cgltf_accessor_read_float(colors, i, c, 4))
            throw std::runtime_error("Current color accessor read failed");
          vertices.push_back({p, {c[0], c[1], c[2], c[3]}});
        }
        std::vector<uint32_t> indices;
        for (size_t i = 0; i < (primitive.indices ? primitive.indices->count
                                                  : position->count);
             ++i)
          indices.push_back(
              uint32_t(primitive.indices
                           ? cgltf_accessor_read_index(primitive.indices, i)
                           : i));
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     GLsizeiptr(vertices.size() * sizeof(Vertex)),
                     vertices.data(), GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void *>(offsetof(Vertex, c)));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     GLsizeiptr(indices.size() * sizeof(uint32_t)),
                     indices.data(), GL_STREAM_DRAW);
        glUniform4fv(glGetUniformLocation(program, "factor"), 1,
                     material.pbr_metallic_roughness.base_color_factor);
        glUniform1i(glGetUniformLocation(program, "mode"),
                    int(material.alpha_mode));
        glUniform1f(glGetUniformLocation(program, "cutoff"),
                    material.alpha_cutoff);
        if (material.double_sided)
          glDisable(GL_CULL_FACE);
        else {
          glEnable(GL_CULL_FACE);
          glCullFace(GL_BACK);
        }
        glFrontFace(GL_CCW);
        if (material.alpha_mode == cgltf_alpha_mode_blend) {
          glEnable(GL_BLEND);
          glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
          glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                              GL_ONE_MINUS_SRC_ALPHA);
          glDepthMask(GL_FALSE);
        } else {
          glDisable(GL_BLEND);
          glDepthMask(GL_TRUE);
        }
        glDrawElements(GL_TRIANGLES, GLsizei(indices.size()), GL_UNSIGNED_INT,
                       nullptr);
      }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(destination));
    glViewport(0, 0, 96, 96);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glUseProgram(resolve);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, linear.t);
    glBindSampler(0, 0);
    glUniform1i(glGetUniformLocation(resolve, "scene"), 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
};
} // namespace polygon_color_consumer
