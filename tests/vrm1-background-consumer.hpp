// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Independent native-current source-view PBR consumer. Original parsed
// geometry/attributes/images; actual persisted current source UV matrices.
// Background factors are evaluated from original expression inputs, not the
// production resolver. The supplied existing paint basis stays independent.
// Other material roles refuse explicitly. No production material/pose helpers.
#include "vrm-role-consumer.hpp"
#include "vrm1-background-paint-oracle.hpp"
struct V1BackgroundConsumer {
  float expression=0;int pose=0;bool wrong_uv=false,wrong_paint=false,wrong_sampler=false,wrong_min=false,wrong_shade_uv=false,wrong_emission_uv=false,wrong_tint=false;
  dome::Material requested;
  std::optional<std::array<float,3>> absolute_background;bool omit_strength=false,scale_physical=false;
  std::array<float,6> current_base{1,0,0,1,0,0},current_emission{1,0,0,1,0,0};
  using J=nlohmann::json;
  GLuint program = 0, vao = 0, vbo = 0, ibo = 0;
  V1BackgroundConsumer() {
    const char *vs = R"GLSL(#version 330 core
layout(location=0)in vec3 p;layout(location=1)in vec3 N;
layout(location=2)in vec2 UV;layout(location=3)in vec2 SU;layout(location=4)in vec2 EU;layout(location=5)in vec2 PU;layout(location=6)in vec4 VC;
uniform mat4 inverseCamera;out vec3 worldPoint;out vec4 vertexColor;
out vec3 n;out vec2 uv,shadeUV,emissionUV,paintUV;
void main(){worldPoint=(inverseCamera*vec4(p,1)).xyz;vertexColor=VC;paintUV=PU;n=N;uv=UV;shadeUV=SU;emissionUV=EU;gl_Position=vec4(p,1);})GLSL";
    const char *fs = R"GLSL(#version 330 core
in vec3 n,worldPoint;in vec2 uv,shadeUV,emissionUV,paintUV;in vec4 vertexColor;out vec4 result;
uniform sampler2D baseMap,emissionMap,paintColor,paintEmission,paintCoverage,paintPhysical;
uniform vec2 wrongPaint;uniform int wrongTint;uniform float physicalMultiplier;
uniform vec4 baseFactor;uniform vec3 emissionFactor;uniform float emissionStrength;
uniform float legacyStrength;
uniform vec3 baseX,baseY,emissionX,emissionY;uniform float metallicFactor,roughnessFactor,cutoff;uniform int alphaMode,unlit;
vec2 mapped(vec2 value,vec3 X,vec3 Y){return vec2(dot(X,vec3(value,1)),dot(Y,vec3(value,1)));}
vec3 light(vec3 N,vec3 V,vec3 L,vec3 irradiance,vec3 albedo){
 vec3 H=normalize(V+L);float NoV=max(dot(N,V),.0001),NoL=max(dot(N,L),0),NoH=max(dot(N,H),0),VoH=max(dot(V,H),0);
 float r=max(roughnessFactor,.04),a=r*r,a2=a*a;float D=a2/(3.141592653589793*pow(NoH*NoH*(a2-1)+1,2));float k=(r+1)*(r+1)/8;
 float G=NoV/(NoV*(1-k)+k)*NoL/(NoL*(1-k)+k);vec3 f0=mix(vec3(.04),albedo,metallicFactor),F=f0+(1-f0)*pow(1-VoH,5);
 return ((1-F)*(1-metallicFactor)*albedo/3.141592653589793+D*G*F/max(4*NoV*NoL,.0001))*irradiance*NoL;
}
void main(){vec2 paintPoint=paintUV+wrongPaint;vec4 coverage=texture(paintCoverage,paintPoint);
 vec4 color=(texture(baseMap,mapped(uv,baseX,baseY))*baseFactor*(1-coverage.r)+texture(paintColor,paintPoint)*(wrongTint!=0?baseFactor:vec4(1)))*vertexColor;
 if(alphaMode==1)color.a=1;else if(alphaMode==2){if(color.a<cutoff)discard;color.a=1;}
 if(unlit!=0){result=color;return;}
 vec3 N=normalize(n);if(!gl_FrontFacing)N=-N;vec3 V=normalize(vec3(0,0,2)-worldPoint);
 vec3 reflected=light(N,V,normalize(vec3(-.4,.8,.7)),vec3(3.8),color.rgb)+light(N,V,normalize(vec3(.7,.2,-.4)),vec3(.65,.8,1),color.rgb)+.045*color.rgb;
 vec3 emitted=texture(emissionMap,mapped(emissionUV,emissionX,emissionY)).rgb*emissionFactor*emissionStrength*(1-coverage.a)+texture(paintEmission,paintPoint).rgb*legacyStrength+texture(paintPhysical,paintPoint).rgb*physicalMultiplier;
 vec3 linearColor=reflected+emitted;result=vec4(linearColor/(linearColor+1),color.a);
})GLSL";
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
  ~V1BackgroundConsumer() {
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
    if(wrong_sampler||wrong_min)glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    if(wrong_sampler)glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glUniform1i(loc(uniform), unit);
    return id;
  }
  void transform(const J &info, const char *x, const char *y,const J &source,uint32_t material) {
    auto values=std::string_view(x)=="emissionX"?current_emission:current_base;if(wrong_uv)values={1,0,0,1,0,0};
    glUniform3f(loc(x),values[0],values[1],values[4]);glUniform3f(loc(y),values[2],values[3],values[5]);
  }

  Pixels render(role_consumer::Model &model, double seconds,
                role_consumer::Matrix prefix,const dome::SculptObject &native) {
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
    role_consumer::Matrix inverse{1/prefix[0],0,0,0,0,1/prefix[5],0,0,0,0,1/prefix[10],0,-prefix[12]/prefix[0],-prefix[13]/prefix[5],-prefix[14]/prefix[10],1};
    if(prefix[1]||prefix[2]||prefix[3]||prefix[4]||prefix[6]||prefix[7]||prefix[8]||prefix[9]||prefix[11]||prefix[15]!=1)throw std::runtime_error("PBR consumer camera is not its declared orthographic domain");
    glUniformMatrix4fv(loc("inverseCamera"),1,GL_FALSE,inverse.data());glUniform1i(loc("alphaMode"),int(requested.alpha));scalar("cutoff",requested.alpha_cutoff);glUniform1i(loc("unlit"),requested.unlit);
    if(requested.alpha==dome::Material::AlphaMode::Blend||requested.alpha==dome::Material::AlphaMode::Automatic){glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);}

    const auto head=model.json.at("extensions").at("VRMC_vrm").at("humanoid").at("humanBones").at("head").at("node").get<size_t>();auto &joint=model.data->nodes[head];const auto &original=model.json.at("nodes").at(head);
    if(original.contains("matrix")||original.contains("rotation"))throw std::runtime_error("Independent posed expression fixture requires its actual unrotated TRS head");
    const auto translation=original.value("translation",std::array<float,3>{});const float angle=pose==1?.14f:pose==2?-.12f:0;
    const std::array<float,3> delta=pose==1?std::array<float,3>{.015f,0,0}:pose==2?std::array<float,3>{-.02f,.01f,0}:std::array<float,3>{};
    joint.has_translation=joint.has_rotation=1;for(size_t k=0;k<3;++k)joint.translation[k]=translation[k]+delta[k];joint.rotation[0]=0;joint.rotation[1]=pose==2?std::sin(angle*.5f):0;joint.rotation[2]=pose==1?std::sin(angle*.5f):0;joint.rotation[3]=std::cos(angle*.5f);
    BackgroundPaintOracle paint(*native.painted);const auto paint_images=paint.upload(program);
    glUniform2f(loc("wrongPaint"),wrong_paint?expression*.2f:0,wrong_paint?-expression*.1f:0);glUniform1i(loc("wrongTint"),wrong_tint);
    for (size_t ni = 0; ni < model.data->nodes_count; ++ni) {
      if(int(ni)!=native.appearance->source_node)continue;
      auto &node = model.data->nodes[ni];
      if (!node.mesh)
        continue;

      role_consumer::Matrix W{};
      cgltf_node_transform_world(&node, W.data());
      W = role_consumer::multiply(prefix, W);
      auto NW = role_consumer::normal_matrix(W);
      for (size_t pi = 0; pi < node.mesh->primitives_count; ++pi) {
        if(int(pi)!=native.appearance->source_primitive)continue;
        auto &p = node.mesh->primitives[pi];
        if (!p.material)
          throw std::runtime_error("V1 oracle requires authored material");
        auto &m = model.json.at("materials")
                      .at(size_t(p.material - model.data->materials));
        auto &ext = m.at("extensions").at("VRMC_materials_mtoon");
        if (m.value("alphaMode", std::string("OPAQUE")) != "OPAQUE" ||
            !m.value("doubleSided", false))
          throw std::runtime_error(
              "Independent painted oracle requires its actual opaque "
              "double-sided static primitives");
        // Core occlusion/MR fields are PBR fallbacks, not MToon1 roles.
        for (const char *role :
             {"normalTexture"})
          if (m.contains(role))
            throw std::runtime_error("Unexpected independent V1 role");
        for (const char *role :
             {"matcapTexture", "rimMultiplyTexture",
              "outlineWidthMultiplyTexture", "shadingShiftTexture",
              "uvAnimationMaskTexture"})
          if (ext.contains(role))
            throw std::runtime_error("Unexpected independent V1 MToon role");
        auto &base = m.at("pbrMetallicRoughness");
        scalar("metallicFactor",base.value("metallicFactor",1.f));scalar("roughnessFactor",base.value("roughnessFactor",1.f));
        if(base.contains("metallicRoughnessTexture")||m.contains("occlusionTexture"))throw std::runtime_error("PBR consumer requires this genuine constant ORM domain");
        auto &bi = base.at("baseColorTexture");
        auto &si = ext.at("shadeMultiplyTexture");
        const auto &ei=m.at("emissiveTexture");
        auto ef=m.value("emissiveFactor",std::array<float,3>{});
        
        float strength=1;if(m.contains("extensions")&&m.at("extensions").contains("KHR_materials_emissive_strength"))strength=m.at("extensions").at("KHR_materials_emissive_strength").value("emissiveStrength",1.f);
        scalar("emissionStrength",strength);scalar("legacyStrength",omit_strength?1:strength);scalar("physicalMultiplier",scale_physical?strength:1);
        auto baseColor =
            base.value("baseColorFactor", std::array<float, 4>{1, 1, 1, 1});
        auto shadeColor =
            ext.value("shadeColorFactor", std::array<float, 3>{0, 0, 0});
        for(const auto &bind:model.json.at("extensions").at("VRMC_vrm").at("expressions").at("preset").at("happy").value("materialColorBinds",J::array()))if(bind.at("material").get<int>()==native.appearance->source_material){const auto type=bind.at("type").get<std::string>();const auto target=bind.at("targetValue").get<std::array<float,4>>();if(type=="color")for(size_t k=0;k<4;++k)baseColor[k]+=expression*(target[k]-baseColor[k]);else if(type=="shadeColor")for(size_t k=0;k<3;++k)shadeColor[k]+=expression*(target[k]-shadeColor[k]);else if(type=="emissionColor")for(size_t k=0;k<3;++k)ef[k]+=expression*(target[k]-ef[k]);else throw std::runtime_error("Independent static expression color role not implemented");}
        if(absolute_background)for(size_t k=0;k<3;++k)ef[k]=absolute_background->at(k)/strength;
        glUniform3fv(loc("emissionFactor"),1,ef.data());
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
        transform(bi, "baseX", "baseY",model.json,uint32_t(native.appearance->source_material));
        transform(si, "shadeX", "shadeY",model.json,uint32_t(native.appearance->source_material));
        transform(ei, "emissionX", "emissionY",model.json,uint32_t(native.appearance->source_material));
        auto bt = texture(model.data, bi, 0, "baseMap"),
             st = texture(model.data, si, 1, "shadeMap"),
             et = texture(model.data, ei, 2, "emissionMap");
        auto P = role_consumer::attr(p.attributes, p.attributes_count,
                                     cgltf_attribute_type_position);
        auto N = role_consumer::attr(p.attributes, p.attributes_count,
                                     cgltf_attribute_type_normal);
        if (!P || !N)
          throw std::runtime_error("V1 authored geometry missing");
        struct Vertex {
          Vec3 p, n;
          std::array<float, 2> uv, shade, emission,paint;std::array<float,4> color;
        };
        std::vector<Vertex> vertices;
        std::vector<role_consumer::Matrix> skin;
        std::vector<role_consumer::NormalMatrix> skin_normal;
        if(node.skin)for(size_t joint=0;joint<node.skin->joints_count;++joint){
          role_consumer::Matrix w{},ibm{};cgltf_node_transform_world(node.skin->joints[joint],w.data());
          if(!node.skin->inverse_bind_matrices||!cgltf_accessor_read_float(node.skin->inverse_bind_matrices,joint,ibm.data(),16))throw std::runtime_error("Independent V1 IBM missing");
          auto matrix=role_consumer::multiply(prefix,role_consumer::multiply(w,ibm));skin.push_back(matrix);skin_normal.push_back(role_consumer::normal_matrix(matrix));
        }
        for (size_t corner = 0; corner < native.painted->mesh().indices.size(); ++corner){
          const auto native_vertex=native.painted->mesh().indices[corner];const auto v=native.appearance->source_vertices.at(native_vertex);
          if((p.indices?cgltf_accessor_read_index(p.indices,corner):corner)!=v)throw std::runtime_error("Independent original/current ordered corner witness differs");
          auto position=role_consumer::vec(P,v),normal=role_consumer::vec(N,v);
          for(size_t target=0;target<p.targets_count;++target){
            float weight=0;for(const auto &bind:model.json.at("extensions").at("VRMC_vrm").at("expressions").at("preset").at("happy").value("morphTargetBinds",J::array()))if(bind.at("node").get<size_t>()==ni&&bind.at("index").get<size_t>()==target)weight+=expression*bind.at("weight").get<float>();
            if(auto a=role_consumer::attr(p.targets[target].attributes,p.targets[target].attributes_count,cgltf_attribute_type_position))position+=role_consumer::vec(a,v)*weight;
            if(auto a=role_consumer::attr(p.targets[target].attributes,p.targets[target].attributes_count,cgltf_attribute_type_normal))normal+=role_consumer::vec(a,v)*weight;
          }
          if(node.skin){Vec3 wp{},wn{};for(int set=0;;++set){
            auto js=role_consumer::attr(p.attributes,p.attributes_count,cgltf_attribute_type_joints,set),ws=role_consumer::attr(p.attributes,p.attributes_count,cgltf_attribute_type_weights,set);if(!js)break;
            float j[4],w[4];if(!ws||!cgltf_accessor_read_float(js,v,j,4)||!cgltf_accessor_read_float(ws,v,w,4))throw std::runtime_error("Independent V1 skin accessor missing");
            for(size_t k=0;k<4;++k)if(w[k]){wp+=role_consumer::point(skin.at(size_t(j[k])),position)*w[k];wn+=role_consumer::direction(skin_normal.at(size_t(j[k])),normal)*w[k];}
          }position=wp;normal=wn;}else{position=role_consumer::point(W,position);normal=role_consumer::direction(NW,normal);}
          std::array<float,4> color{1,1,1,1};if(const auto attribute=role_consumer::attr(p.attributes,p.attributes_count,cgltf_attribute_type_color))if(!cgltf_accessor_read_float(attribute,v,color.data(),4))throw std::runtime_error("Independent authored COLOR_0 missing");
          vertices.push_back({position,normal,coordinates(p,bi,v),coordinates(p,si,v),coordinates(p,ei,v),native.painted->mesh().uv.at(native_vertex),color});
        }
        std::vector<uint32_t> indices;
        for (size_t c = 0; c < (p.indices ? p.indices->count : P->count); ++c)
          indices.push_back(uint32_t(c));
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                     vertices.data(), GL_STREAM_DRAW);
        for (GLuint a = 0; a < 7; ++a) {
          glVertexAttribPointer(
              a, a < 2 ? 3 : a==6?4:2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
              reinterpret_cast<void *>(a == 0   ? offsetof(Vertex, p)
                                       : a == 1 ? offsetof(Vertex, n)
                                       : a == 2 ? offsetof(Vertex, uv)
                                       : a == 3 ? offsetof(Vertex, shade)
                                                : a == 4 ? offsetof(Vertex, emission) : a==5?offsetof(Vertex,paint):offsetof(Vertex,color)));
          glEnableVertexAttribArray(a);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t),
                     indices.data(), GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES, GLsizei(indices.size()), GL_UNSIGNED_INT,
                       nullptr);
        glDeleteTextures(1, &bt);
        glDeleteTextures(1, &st);
        glDeleteTextures(1, &et);
      }
    }
    glDeleteTextures(4,paint_images.data());return read();
  }
};
