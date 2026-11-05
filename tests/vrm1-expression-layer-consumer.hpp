// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Independent parsed VRM1 base/shade/emission consumer, extended from the
// pinned-spec static oracle with independently evaluated skins and morphs.
// Other material roles refuse explicitly. No production material/pose helpers.
#include "vrm-role-consumer.hpp"
#include "consumer-handedness.hpp"
#include <map>
struct V1ExpressionLayerConsumer {
  using J=nlohmann::json;
  float expression=0;int pose=0,selected_node=-1;bool wrong_mask_cutoff=false,wrong_paint_uv=false,wrong_base_tint=false,wrong_shared_shade=false;
  bool wrong_source_uv=false,wrong_matcap_view=false,wrong_matcap_uv_target=false,wrong_matcap_color=false,wrong_rim_color=false,wrong_rim_uv=false,wrong_rim_mix=false;
  bool wrong_normal_scale=false,wrong_normal_uv=false,wrong_normal_hand=false,wrong_normal_srgb=false;
  bool wrong_shift_scale=false,wrong_shift_uv=false,wrong_shift_srgb=false,wrong_shift_channel=false;
  std::map<std::string,float> weights;
  struct WeightedExpression {const J *value;float weight;};
  std::vector<WeightedExpression> active;
  GLuint program = 0, vao = 0, vbo = 0, ibo = 0, resolve=0,framebuffer=0,color=0,depth=0;
  V1ExpressionLayerConsumer() {
    const char *vs = R"GLSL(#version 330 core
layout(location=0)in vec3 p;layout(location=1)in vec3 N;
layout(location=2)in vec2 UV;layout(location=3)in vec2 SU;layout(location=4)in vec2 EU;layout(location=5)in vec2 RU;layout(location=6)in vec2 NU;layout(location=7)in vec4 T;layout(location=8)in vec2 HU;
uniform mat4 camera;out vec3 n,world;out vec2 uv,shadeUV,emissionUV,rimUV,normalUV,shiftUV;out vec3 tangent;out float tangentHand;
void main(){n=N;world=p;uv=UV;shadeUV=SU;emissionUV=EU;rimUV=RU;normalUV=NU;shiftUV=HU;tangent=normalize(T.xyz);tangentHand=T.w;gl_Position=camera*vec4(p,1);})GLSL";
    const char *fs = R"GLSL(#version 330 core
in vec3 n,world;in vec2 uv,shadeUV,emissionUV,rimUV,normalUV,shiftUV;in vec3 tangent;in float tangentHand;out vec4 result;
uniform sampler2D baseMap,shadeMap,emissionMap,matcapMap,rimMap,normalMap,shiftMap;uniform int basePresent,shadePresent,emissionPresent,matcapPresent,rimPresent,normalPresent,shiftPresent,wrongMatcapView,wrongShiftChannel;uniform float normalScale,shiftTextureScale;
uniform vec3 matcapFactor,rimFactor,viewOrigin;uniform float rimLightingMix,rimPower,rimLift;
uniform vec3 emissionFactor;uniform float emissionStrength;
uniform int blend;uniform float alphaCutoff;uniform vec4 baseFactor;uniform vec3 shadeFactor;
uniform vec2 scroll;uniform float speed,seconds,shift,toony,equalization;
uniform vec3 baseX,baseY,shadeX,shadeY,emissionX,emissionY,matcapX,matcapY,rimX,rimY,normalX,normalY,shiftX,shiftY;
vec2 animated(vec2 u){float a=speed*seconds;vec2 q=u-.5;
return vec2(cos(a)*q.x+sin(a)*q.y,-sin(a)*q.x+cos(a)*q.y)+.5+scroll*seconds;}
vec2 mapped(vec2 u,vec3 X,vec3 Y){vec2 q=animated(u)*X.xy;float c=cos(X.z),s=sin(X.z);return mat2(c,s,-s,c)*q+Y.xy;}
// Independent pinned UniVRM view-normal lookup, in its left-handed frame.
// Reflect X for glTF -> Unity, then flip texture V back to glTF's top row.
// This expresses coordinate conversion, not a call to the native UV helper.
vec2 viewLookup(vec3 N,vec3 V){vec3 L=vec3(-N.x,N.y,N.z),W=vec3(-V.x,V.y,V.z);
vec3 X=normalize(cross(W,vec3(0,1,0))),Y=normalize(cross(X,W));
vec2 q=.5+.5*vec2(dot(X,L),dot(Y,L));return vec2(q.x,1-q.y);}
void main(){vec3 N=normalize(n);if(!gl_FrontFacing)N=-N;
if(normalPresent==1){vec3 q=texture(normalMap,mapped(normalUV,normalX,normalY)).rgb*2-1;q.xy*=normalScale;vec3 T=normalize(tangent),B=normalize(cross(N,T))*tangentHand;N=normalize(T*q.x+B*q.y+N*q.z);}
vec4 lit=(basePresent==1?texture(baseMap,mapped(uv,baseX,baseY)):vec4(1))*baseFactor;
vec3 shade=(shadePresent==1?texture(shadeMap,mapped(shadeUV,shadeX,shadeY)).rgb:vec3(1))*shadeFactor;
vec4 shiftTexel=shiftPresent==1?texture(shiftMap,mapped(shiftUV,shiftX,shiftY)):vec4(0);
float width=1-toony,x=dot(N,normalize(vec3(-.4,.8,.7)))+shift+(wrongShiftChannel==1?shiftTexel.g:shiftTexel.r)*shiftTextureScale;
float ratio=width<=.0000001?step(0,x):clamp((x+width)/(2*width),0,1);
float rawGi=mix(.1,.25,.5+.5*N.y),gi=mix(rawGi,.175,equalization);
vec3 emission=(emissionPresent==1?texture(emissionMap,mapped(emissionUV,emissionX,emissionY)).rgb:vec3(1))*emissionFactor*emissionStrength;
vec3 V=normalize(viewOrigin-world);vec2 M=wrongMatcapView==1?uv:viewLookup(N,V);
vec3 matcap=matcapPresent==1?texture(matcapMap,mapped(M,matcapX,matcapY)).rgb*matcapFactor:vec3(0);
vec3 rim=rimFactor*pow(clamp(1-dot(N,V)+rimLift,0.,1.),max(rimPower,.00001));
matcap=(matcap+rim)*(rimPresent==1?texture(rimMap,mapped(rimUV,rimX,rimY)).rgb:vec3(1))*mix(1.,1.+rawGi,rimLightingMix);
// Evaluate all implicitly filtered role textures before divergent discard.
// A removed quad lane cannot define the mip derivatives of later samples.
if(blend==2&&lit.a<alphaCutoff)discard;
result=vec4(mix(shade,lit.rgb,ratio)+gi*lit.rgb+emission+matcap,blend==1?lit.a:1);})GLSL";
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
    const char *rv=R"GLSL(#version 330 core
out vec2 uv;void main(){uv=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(uv*2-1,0,1);})GLSL";
    const char *rf=R"GLSL(#version 330 core
in vec2 uv;out vec4 result;uniform sampler2D scene;void main(){result=texture(scene,uv);})GLSL";
    auto a=role_consumer::shader(GL_VERTEX_SHADER,rv),b=role_consumer::shader(GL_FRAGMENT_SHADER,rf);resolve=glCreateProgram();glAttachShader(resolve,a);glAttachShader(resolve,b);glLinkProgram(resolve);glDeleteShader(a);glDeleteShader(b);glGetProgramiv(resolve,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Independent layer resolve failed");
    GLint destination=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&destination);glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,96,96,0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,96,96);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Independent layer HDR framebuffer incomplete");glBindFramebuffer(GL_FRAMEBUFFER,GLuint(destination));
  }
  ~V1ExpressionLayerConsumer() {
    glDeleteBuffers(1, &ibo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);glDeleteProgram(resolve);glDeleteFramebuffers(1,&framebuffer);glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&color);
  }
  GLint loc(const char *s) { return glGetUniformLocation(program, s); }
  void scalar(const char *name, float value) { glUniform1f(loc(name), value); }
  std::array<float, 2> coordinates(const cgltf_primitive &p, const J &info,
                                   size_t v) {
    if(info.is_null())return {};
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
    const char *presence=unit==0?"basePresent":unit==1?"shadePresent":unit==2?"emissionPresent":unit==3?"matcapPresent":unit==4?"rimPresent":unit==5?"normalPresent":"shiftPresent";
    glUniform1i(loc(presence),!info.is_null());glUniform1i(loc(uniform),unit);
    if(info.is_null()){glActiveTexture(GL_TEXTURE0+unit);glBindSampler(unit,0);glBindTexture(GL_TEXTURE_2D,0);return 0;}
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
    glTexImage2D(GL_TEXTURE_2D, 0, ((unit==5&&!wrong_normal_srgb)||(unit==6&&!wrong_shift_srgb))?GL_RGBA8:GL_SRGB8_ALPHA8, image.width, image.height,
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
  void transform(const J &info, const char *x, const char *y,const J &expression_data,size_t material,bool paint_layer=false,bool screen_mapped=false) {
    J t = J::object();
    if (info.contains("extensions") &&
        info.at("extensions").contains("KHR_texture_transform"))
      t = info.at("extensions").at("KHR_texture_transform");
    auto offset = t.value("offset", std::array<float, 2>{0, 0}),
         scale = t.value("scale", std::array<float, 2>{1, 1});
    double a = t.value("rotation", 0.0);
    const auto initial_scale=scale,initial_offset=offset;
    if(!screen_mapped){if(weights.empty()){
      for(const auto &bind:expression_data.value("textureTransformBinds",J::array()))if(bind.at("material").get<size_t>()==material)for(size_t k=0;k<2;++k){scale[k]+=float((double(bind.value("scale",std::array<float,2>{1,1})[k])-initial_scale[k])*expression);offset[k]+=float((double(bind.value("offset",std::array<float,2>{})[k])-initial_offset[k])*expression);}
    }else{double total=0;std::array<double,2> scales{},offsets{};for(const auto &e:active)for(const auto &bind:e.value->value("textureTransformBinds",J::array()))if(bind.at("material").get<size_t>()==material){total+=e.weight;const auto target_scale=bind.value("scale",std::array<float,2>{1,1}),target_offset=bind.value("offset",std::array<float,2>{});for(size_t k=0;k<2;++k){scales[k]+=double(target_scale[k])*e.weight;offsets[k]+=double(target_offset[k])*e.weight;}}for(size_t k=0;k<2;++k){scale[k]=float(initial_scale[k]*(1-total)+scales[k]);offset[k]=float(initial_offset[k]*(1-total)+offsets[k]);}}
    }
    if(wrong_source_uv&&!paint_layer&&!screen_mapped){scale=initial_scale;offset=initial_offset;}
    if(wrong_paint_uv&&std::string_view(x)=="baseX"&&paint_layer){scale=initial_scale;offset=initial_offset;offset[0]+=expression*.2f;offset[1]-=expression*.1f;}
    glUniform3f(loc(x),scale[0],scale[1],float(a));glUniform3f(loc(y),offset[0],offset[1],0);
  }
  Pixels render(role_consumer::Model &model, double seconds,
                role_consumer::Matrix prefix) {
    GLint destination=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&destination);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    const auto &expression_data=model.json.at("extensions").at("VRMC_vrm").at("expressions").at("preset").at("happy");
    active.clear();size_t selected=0;if(!weights.empty()){const auto &definitions=model.json.at("extensions").at("VRMC_vrm").at("expressions");for(const char *table:{"preset","custom"})if(definitions.contains(table))for(const auto &entry:definitions.at(table).items()){const auto key=std::string(table)+"/"+entry.key();if(const auto at=weights.find(key);at!=weights.end()){if(!std::isfinite(at->second)||at->second<0||at->second>1||entry.value().value("isBinary",false))throw std::runtime_error("Independent target oracle requires finite nonbinary requested weights");if(std::string_view(table)=="preset"&&entry.key()!="happy")throw std::runtime_error("Independent target oracle does not simulate procedural expression groups");active.push_back({&entry.value(),at->second});++selected;}}if(selected!=weights.size())throw std::runtime_error("Independent target oracle requested an absent source expression");}
    std::vector<std::vector<cgltf_float>> node_weights(model.data->nodes_count);
    struct WeightRestore {cgltf_data *data;std::vector<std::pair<cgltf_float*,cgltf_size>> pointers;~WeightRestore(){for(size_t i=0;i<pointers.size();++i){data->nodes[i].weights=pointers[i].first;data->nodes[i].weights_count=pointers[i].second;}}} restore{model.data,{}};for(size_t i=0;i<model.data->nodes_count;++i)restore.pointers.push_back({model.data->nodes[i].weights,model.data->nodes[i].weights_count});
    for(size_t ni=0;ni<model.data->nodes_count;++ni){auto &node=model.data->nodes[ni];if(!node.mesh)continue;const auto &json=model.json.at("nodes").at(ni);const size_t count=node.mesh->primitives[0].targets_count;auto &values=node_weights[ni];values.assign(count,0);const auto defaults=json.value("weights",model.json.at("meshes").at(size_t(node.mesh-model.data->meshes)).value("weights",J::array()));for(size_t t=0;t<count&&t<defaults.size();++t)values[t]=defaults.at(t).get<float>();node.weights=values.data();node.weights_count=count;}
    // Only genuinely bound node/target pairs reset, across all expressions.
    const auto &all=model.json.at("extensions").at("VRMC_vrm").at("expressions");for(const char *table:{"preset","custom"})if(all.contains(table))for(const auto &e:all.at(table))for(const auto &bind:e.value("morphTargetBinds",J::array()))node_weights.at(bind.at("node").get<size_t>()).at(bind.at("index").get<size_t>())=0;
    if(weights.empty()){for(const auto &bind:expression_data.value("morphTargetBinds",J::array()))node_weights.at(bind.at("node").get<size_t>()).at(bind.at("index").get<size_t>())+=bind.at("weight").get<float>()*expression;}else for(const auto &e:active)for(const auto &bind:e.value->value("morphTargetBinds",J::array()))node_weights.at(bind.at("node").get<size_t>()).at(bind.at("index").get<size_t>())+=bind.at("weight").get<float>()*e.weight;
    const size_t head=model.json.at("extensions").at("VRMC_vrm").at("humanoid").at("humanBones").at("head").at("node").get<size_t>();auto &joint=model.data->nodes[head];const auto &raw=model.json.at("nodes").at(head);cgltf_node temp{};temp.has_matrix=raw.contains("matrix");temp.has_translation=raw.contains("translation");temp.has_rotation=raw.contains("rotation");temp.has_scale=raw.contains("scale");auto t=raw.value("translation",std::array<float,3>{});auto q=raw.value("rotation",std::array<float,4>{0,0,0,1});auto z=raw.value("scale",std::array<float,3>{1,1,1});auto m=raw.value("matrix",std::array<float,16>{});std::copy(t.begin(),t.end(),temp.translation);std::copy(q.begin(),q.end(),temp.rotation);std::copy(z.begin(),z.end(),temp.scale);std::copy(m.begin(),m.end(),temp.matrix);cgltf_node_transform_local(&temp,joint.matrix);joint.has_matrix=1;const float angle=pose==2?-.12f:0,c=std::cos(angle),si=std::sin(angle);if(pose==2){for(size_t col=0;col<3;++col){const float x=joint.matrix[col*4],y=joint.matrix[col*4+2];joint.matrix[col*4]=c*x+si*y;joint.matrix[col*4+2]=-si*x+c*y;}joint.matrix[12]-=.02f;joint.matrix[13]+=.01f;}
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
    glUseProgram(program);glUniformMatrix4fv(loc("camera"),1,GL_FALSE,prefix.data());
    for (size_t ni = 0; ni < model.data->nodes_count; ++ni) {
      auto &node = model.data->nodes[ni];
      if (selected_node>=0&&int(ni)!=selected_node)continue;
      if (!node.mesh)
        continue;

      role_consumer::Matrix W{};
      cgltf_node_transform_world(&node, W.data());

      auto NW = role_consumer::normal_matrix(W);
      for (size_t pi = 0; pi < node.mesh->primitives_count; ++pi) {
        auto &p = node.mesh->primitives[pi];
        if (!p.material)
          throw std::runtime_error("V1 oracle requires authored material");
        auto m = model.json.at("materials")
                      .at(size_t(p.material - model.data->materials));
        auto &ext = m.at("extensions").at("VRMC_materials_mtoon");
        if(!ext.contains("matcapFactor"))ext["matcapFactor"]=std::array<float,3>{1,1,1};
        if(!ext.contains("parametricRimColorFactor"))ext["parametricRimColorFactor"]=std::array<float,3>{0,0,0};
        const auto alpha=m.value("alphaMode",std::string("OPAQUE"));const bool blended=alpha=="BLEND",masked=alpha=="MASK";
        if((!blended&&!masked&&alpha!="OPAQUE")||!m.value("doubleSided",false))throw std::runtime_error("Independent expression-layer oracle requires opaque/blend double-sided source");
        if(blended){glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glDepthFunc(GL_LEQUAL);}else{glDisable(GL_BLEND);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);}glUniform1i(loc("blend"),blended?1:masked?2:0);scalar("alphaCutoff",wrong_mask_cutoff?.99f:m.value("alphaCutoff",.5f));
        const size_t material=size_t(p.material-model.data->materials);
        if(weights.empty()){
         for(const auto &bind:expression_data.value("materialColorBinds",J::array()))if(bind.at("material").get<size_t>()==material){const std::string type=bind.at("type");J *field=type=="color"?&m["pbrMetallicRoughness"]["baseColorFactor"]:type=="shadeColor"?&m["extensions"]["VRMC_materials_mtoon"]["shadeColorFactor"]:type=="emissionColor"?&m["emissiveFactor"]:type=="rimColor"?&m["extensions"]["VRMC_materials_mtoon"]["parametricRimColorFactor"]:type=="matcapColor"?&m["extensions"]["VRMC_materials_mtoon"]["matcapFactor"]:nullptr;if(!field)throw std::runtime_error("Independent layer colour type unsupported");for(size_t k=0;k<field->size();++k)(*field)[k]=field->at(k).get<double>()+(bind.at("targetValue").at(k).get<double>()-field->at(k).get<double>())*expression;}
        }else{for(const char *type:{"color","shadeColor","emissionColor","matcapColor","rimColor"}){J &field=std::string_view(type)=="color"?m["pbrMetallicRoughness"]["baseColorFactor"]:std::string_view(type)=="shadeColor"?m["extensions"]["VRMC_materials_mtoon"]["shadeColorFactor"]:std::string_view(type)=="emissionColor"?m["emissiveFactor"]:std::string_view(type)=="matcapColor"?m["extensions"]["VRMC_materials_mtoon"]["matcapFactor"]:m["extensions"]["VRMC_materials_mtoon"]["parametricRimColorFactor"];const auto initial=field;for(size_t k=0;k<field.size();++k){double value=initial.at(k).get<double>();for(const auto &e:active)for(const auto &bind:e.value->value("materialColorBinds",J::array()))if(bind.at("material").get<size_t>()==material&&bind.at("type")==type)value+=(bind.at("targetValue").at(k).get<double>()-initial.at(k).get<double>())*e.weight;field[k]=float(value);}}}
        if(wrong_base_tint&&pi>0){const auto &bind=expression_data.at("materialColorBinds").at(0);for(size_t k=0;k<3;++k)m["pbrMetallicRoughness"]["baseColorFactor"][k]=1+(bind.at("targetValue").at(k).get<double>()-1)*expression;}
        // Core occlusion/MR fields are PBR fallbacks, not MToon1 roles.
        for (const char *role :
             {"outlineWidthMultiplyTexture", "uvAnimationMaskTexture"})
          if (ext.contains(role))
            throw std::runtime_error("Unexpected independent V1 MToon role");
        auto &base = m.at("pbrMetallicRoughness");
        const auto bi = base.value("baseColorTexture",J());
        const auto si = ext.value("shadeMultiplyTexture",J());
        const auto ei=m.value("emissiveTexture",J()),mi=ext.value("matcapTexture",J()),ri=ext.value("rimMultiplyTexture",J()),ni=m.value("normalTexture",J()),hi=ext.value("shadingShiftTexture",J());
        const auto ef=m.value("emissiveFactor",std::array<float,3>{});
        glUniform3fv(loc("emissionFactor"),1,ef.data());
        float strength=1;if(m.contains("extensions")&&m.at("extensions").contains("KHR_materials_emissive_strength"))strength=m.at("extensions").at("KHR_materials_emissive_strength").value("emissiveStrength",1.f);
        scalar("emissionStrength",strength);
        auto baseColor =
            base.value("baseColorFactor", std::array<float, 4>{1, 1, 1, 1});
        auto shadeColor =
            ext.value("shadeColorFactor", std::array<float, 3>{1, 1, 1});
        if(wrong_shared_shade&&pi>0)shadeColor={0,0,0};
        glUniform4fv(loc("baseFactor"), 1, baseColor.data());
        glUniform3fv(loc("shadeFactor"), 1, shadeColor.data());
        auto mf=ext.value("matcapFactor",std::array<float,3>{1,1,1});if(wrong_matcap_color)mf={0,0,0};glUniform3fv(loc("matcapFactor"),1,mf.data());glUniform3f(loc("viewOrigin"),0,0,2);scalar("rimLightingMix",wrong_rim_mix?0.f:ext.value("rimLightingMixFactor",1.f));auto rf=ext.value("parametricRimColorFactor",std::array<float,3>{});if(wrong_rim_color)rf={0,0,0};glUniform3fv(loc("rimFactor"),1,rf.data());scalar("rimPower",ext.value("parametricRimFresnelPowerFactor",5.f));scalar("rimLift",ext.value("parametricRimLiftFactor",0.f));glUniform1i(loc("wrongMatcapView"),wrong_matcap_view);
        scalar("seconds", float(seconds));
        scalar("speed", ext.value("uvAnimationRotationSpeedFactor", 0.f));
        glUniform2f(loc("scroll"),
                    ext.value("uvAnimationScrollXSpeedFactor", 0.f),
                    ext.value("uvAnimationScrollYSpeedFactor", 0.f));
        scalar("shift", ext.value("shadingShiftFactor", 0.f));scalar("shiftTextureScale",wrong_shift_scale?0.f:hi.is_null()?1.f:hi.value("scale",1.f));glUniform1i(loc("wrongShiftChannel"),wrong_shift_channel);
        scalar("toony", ext.value("shadingToonyFactor", .9f));
        scalar("equalization", ext.value("giEqualizationFactor", .9f));
        transform(bi, "baseX", "baseY",expression_data,material,pi>0);
        transform(si, "shadeX", "shadeY",expression_data,material);
        transform(ei, "emissionX", "emissionY",expression_data,material);
        transform(mi,"matcapX","matcapY",expression_data,material,false,!wrong_matcap_uv_target);
        transform(ni,"normalX","normalY",expression_data,material);if(wrong_normal_uv)glUniform3f(loc("normalY"),.7f,.8f,0);scalar("normalScale",wrong_normal_scale?0.f:ni.is_null()?1.f:ni.value("scale",1.f));
        transform(hi,"shiftX","shiftY",expression_data,material);if(wrong_shift_uv)glUniform3f(loc("shiftY"),.7f,.8f,0);
        transform(ri,"rimX","rimY",expression_data,material);if(wrong_rim_uv)glUniform3f(loc("rimY"),.7f,.8f,0);
        auto bt = texture(model.data, bi, 0, "baseMap"),
             st = texture(model.data, si, 1, "shadeMap"),
             et = texture(model.data, ei, 2, "emissionMap"),
             mt = texture(model.data,mi,3,"matcapMap"),rt=texture(model.data,ri,4,"rimMap"),nt=texture(model.data,ni,5,"normalMap"),ht=texture(model.data,hi,6,"shiftMap");
        auto P = role_consumer::attr(p.attributes, p.attributes_count,
                                     cgltf_attribute_type_position);
        auto N = role_consumer::attr(p.attributes, p.attributes_count,
                                     cgltf_attribute_type_normal);
        if (!P || !N)
          throw std::runtime_error("V1 authored geometry missing");
        struct Vertex {
          Vec3 p, n;
          std::array<float, 2> uv, shade, emission, rim, normal_uv, shift_uv;std::array<float,4> tangent;
        };
        std::vector<Vertex> vertices;
        std::vector<role_consumer::Matrix> skin;
        std::vector<role_consumer::NormalMatrix> skin_normal;
        if(node.skin)for(size_t joint=0;joint<node.skin->joints_count;++joint){
          role_consumer::Matrix w{},ibm{};cgltf_node_transform_world(node.skin->joints[joint],w.data());
          if(!node.skin->inverse_bind_matrices||!cgltf_accessor_read_float(node.skin->inverse_bind_matrices,joint,ibm.data(),16))throw std::runtime_error("Independent V1 IBM missing");
          auto matrix=role_consumer::multiply(w,ibm);skin.push_back(matrix);skin_normal.push_back(role_consumer::normal_matrix(matrix));
        }
        for (size_t v = 0; v < P->count; ++v){
          auto position=role_consumer::vec(P,v),normal=role_consumer::vec(N,v);float tv[4]{1,0,0,1};
          const auto tangent_attribute=role_consumer::attr(p.attributes,p.attributes_count,cgltf_attribute_type_tangent);
          if(!ni.is_null()&&(!tangent_attribute||!cgltf_accessor_read_float(tangent_attribute,v,tv,4)))throw std::runtime_error("Independent normal role requires genuine original TANGENT");Vec3 tangent{tv[0],tv[1],tv[2]};float hand=consumer_frame::handedness(W);
          for(size_t target=0;target<p.targets_count;++target){
            float weight=node.weights_count?node.weights[target]:node.mesh->weights_count?node.mesh->weights[target]:0;
            if(auto a=role_consumer::attr(p.targets[target].attributes,p.targets[target].attributes_count,cgltf_attribute_type_position))position+=role_consumer::vec(a,v)*weight;
            if(auto a=role_consumer::attr(p.targets[target].attributes,p.targets[target].attributes_count,cgltf_attribute_type_normal))normal+=role_consumer::vec(a,v)*weight;
            if(auto a=role_consumer::attr(p.targets[target].attributes,p.targets[target].attributes_count,cgltf_attribute_type_tangent))tangent+=role_consumer::vec(a,v)*weight;
          }
          if(node.skin){Vec3 wp{},wn{},wt{};role_consumer::Matrix blended{};for(int set=0;;++set){
            auto js=role_consumer::attr(p.attributes,p.attributes_count,cgltf_attribute_type_joints,set),ws=role_consumer::attr(p.attributes,p.attributes_count,cgltf_attribute_type_weights,set);if(!js)break;
            float j[4],w[4];if(!ws||!cgltf_accessor_read_float(js,v,j,4)||!cgltf_accessor_read_float(ws,v,w,4))throw std::runtime_error("Independent V1 skin accessor missing");
            for(size_t k=0;k<4;++k)if(w[k]){wp+=role_consumer::point(skin.at(size_t(j[k])),position)*w[k];wn+=role_consumer::direction(skin_normal.at(size_t(j[k])),normal)*w[k];wt+=role_consumer::direction(skin.at(size_t(j[k])),tangent)*w[k];for(size_t c=0;c<16;++c)blended[c]+=skin.at(size_t(j[k]))[c]*w[k];}
          }position=wp;normal=wn;tangent=wt;hand=consumer_frame::handedness(blended);}else{tangent=role_consumer::direction(W,tangent);position=role_consumer::point(W,position);normal=role_consumer::direction(NW,normal);}
          if(!ni.is_null())normal=normalized(normal);vertices.push_back({position,normal,coordinates(p,bi,v),coordinates(p,si,v),coordinates(p,ei,v),coordinates(p,ri,v),coordinates(p,ni,v),coordinates(p,hi,v),{tangent.x,tangent.y,tangent.z,tv[3]*hand*(wrong_normal_hand?-1.f:1.f)}});
        }
        std::vector<uint32_t> indices;
        for (size_t c = 0; c < (p.indices ? p.indices->count : P->count); ++c)
          indices.push_back(p.indices ? cgltf_accessor_read_index(p.indices, c)
                                      : c);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                     vertices.data(), GL_STREAM_DRAW);
        for (GLuint a = 0; a < 6; ++a) {
          glVertexAttribPointer(
              a, a < 2 ? 3 : 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
              reinterpret_cast<void *>(a == 0   ? offsetof(Vertex, p)
                                       : a == 1 ? offsetof(Vertex, n)
                                       : a == 2 ? offsetof(Vertex, uv)
                                       : a == 3 ? offsetof(Vertex, shade)
                                       : a == 4 ? offsetof(Vertex, emission)
                                                : offsetof(Vertex, rim)));
          glEnableVertexAttribArray(a);
        }
        glVertexAttribPointer(6,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,normal_uv)));glEnableVertexAttribArray(6);
        glVertexAttribPointer(7,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,tangent)));glEnableVertexAttribArray(7);
        glVertexAttribPointer(8,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,shift_uv)));glEnableVertexAttribArray(8);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t),
                     indices.data(), GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES, GLsizei(indices.size()), GL_UNSIGNED_INT,
                       nullptr);
        glDeleteTextures(1, &bt);
        glDeleteTextures(1, &st);
        glDeleteTextures(1, &et);
        glDeleteTextures(1,&mt);glDeleteTextures(1,&rt);glDeleteTextures(1,&nt);glDeleteTextures(1,&ht);
      }
    }
    glBindFramebuffer(GL_FRAMEBUFFER,GLuint(destination));glViewport(0,0,96,96);glDisable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glUseProgram(resolve);glUniform1i(glGetUniformLocation(resolve,"scene"),0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,color);glBindSampler(0,0);glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);
    return read();
  }
};
