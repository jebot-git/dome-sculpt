// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Independent parsed-file consumer. No production frame, morph or skin helper
// is used. Fixture lighting constants match the editor so pixel comparisons
// isolate attribute transport and the versioned TBN evaluation.
#include "cgltf.h"
#include <png.h>
#include <fstream>
#include <map>
namespace frame_consumer {
using namespace dome;
using Matrix=std::array<float,16>;
inline const cgltf_accessor *attr(const cgltf_attribute *a,size_t count,cgltf_attribute_type type,int index=0){for(size_t n=0;n<count;++n)if(a[n].type==type&&a[n].index==index)return a[n].data;return nullptr;}
inline Vec3 vec(const cgltf_accessor*a,size_t i){float v[4]{};if(!a||!cgltf_accessor_read_float(a,i,v,4))throw std::runtime_error("Consumer vector accessor is missing");return {v[0],v[1],v[2]};}
inline Matrix multiply(const Matrix&a,const Matrix&b){Matrix m{};for(size_t c=0;c<4;++c)for(size_t r=0;r<4;++r)for(size_t k=0;k<4;++k)m[c*4+r]+=a[k*4+r]*b[c*4+k];return m;}
inline Vec3 direction(const Matrix&m,Vec3 v){return {m[0]*v.x+m[4]*v.y+m[8]*v.z,m[1]*v.x+m[5]*v.y+m[9]*v.z,m[2]*v.x+m[6]*v.y+m[10]*v.z};}
inline Vec3 point(const Matrix&m,Vec3 v){return direction(m,v)+Vec3{m[12],m[13],m[14]};}
using NormalMatrix=std::array<double,9>;
inline NormalMatrix normal_matrix(const Matrix&m){
 double a[3][6]{};for(size_t r=0;r<3;++r){for(size_t c=0;c<3;++c)a[r][c]=m[c*4+r];a[r][r+3]=1;}
 // Gauss-Jordan independently derives inverse-transpose, rather than reusing
 // the production cofactor implementation.
 for(size_t c=0;c<3;++c){size_t pivot=c;for(size_t r=c+1;r<3;++r)if(std::abs(a[r][c])>std::abs(a[pivot][c]))pivot=r;if(std::abs(a[pivot][c])<1e-12)throw std::runtime_error("Consumer joint matrix is singular");for(size_t k=0;k<6;++k)std::swap(a[c][k],a[pivot][k]);double divisor=a[c][c];for(size_t k=0;k<6;++k)a[c][k]/=divisor;for(size_t r=0;r<3;++r)if(r!=c){double factor=a[r][c];for(size_t k=0;k<6;++k)a[r][k]-=a[c][k]*factor;}}
 NormalMatrix result{};for(size_t r=0;r<3;++r)for(size_t c=0;c<3;++c)result[c*3+r]=a[c][r+3];return result;
}
inline Vec3 direction(const NormalMatrix&m,Vec3 v){return {float(m[0]*v.x+m[3]*v.y+m[6]*v.z),float(m[1]*v.x+m[4]*v.y+m[7]*v.z),float(m[2]*v.x+m[5]*v.y+m[8]*v.z)};}
inline GLuint shader(GLenum type,const char *source){GLuint id=glCreateShader(type);glShaderSource(id,1,&source,nullptr);glCompileShader(id);GLint ok;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192]{};glGetShaderInfoLog(id,sizeof(log),nullptr,log);throw std::runtime_error(log);}return id;}
inline Image image(const cgltf_texture *texture){if(!texture||!texture->image||!texture->image->buffer_view)throw std::runtime_error("Consumer normal image is not embedded");auto *view=texture->image->buffer_view;auto *bytes=static_cast<const uint8_t*>(view->buffer->data)+view->offset;png_image png{};png.version=PNG_IMAGE_VERSION;if(!png_image_begin_read_from_memory(&png,bytes,view->size))throw std::runtime_error("Consumer PNG header failed");png.format=PNG_FORMAT_RGBA;Image out{png.width,png.height,std::vector<uint8_t>(PNG_IMAGE_SIZE(png))};bool ok=png_image_finish_read(&png,nullptr,out.rgba.data(),0,nullptr);png_image_free(&png);if(!ok)throw std::runtime_error("Consumer PNG pixel decode failed");return out;}
inline float linear(float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
struct Renderer {
 GLuint program=0,vao=0,vbo=0,ibo=0;size_t vertices=0,triangles=0;
 Renderer(){
  const char *vs=R"GLSL(#version 330 core
layout(location=0)in vec3 p;layout(location=1)in vec3 N;layout(location=2)in vec4 T;layout(location=3)in vec2 UV;
out vec3 world,n,t,b;out vec2 uv;
void main(){world=p;n=normalize(N);t=normalize(T.xyz);b=cross(n,t)*T.w;uv=UV;gl_Position=vec4(p,1);})GLSL";
  const char *fs=R"GLSL(#version 330 core
in vec3 world,n,t,b;in vec2 uv;out vec4 result;
uniform sampler2D normalMap;uniform float normalScale,roughness,metallic,shift,toony;uniform int legacy;
uniform vec3 baseColor,shadeColor;
vec3 srgb(vec3 x){return mix(1.055*pow(max(x,vec3(0)),vec3(1./2.4))-.055,12.92*x,lessThanEqual(x,vec3(.0031308)));}
vec3 brdf(vec3 N,vec3 V,vec3 L,vec3 E){vec3 H=normalize(V+L);float nv=max(dot(N,V),.0001),nl=max(dot(N,L),0),nh=max(dot(N,H),0),vh=max(dot(V,H),0);float r=max(roughness,.04),a=r*r,aa=a*a;float D=aa/(3.141592653589793*pow(nh*nh*(aa-1)+1,2));float k=(r+1)*(r+1)/8;float G=(nv/(nv*(1-k)+k))*(nl/(nl*(1-k)+k));vec3 f0=mix(vec3(.04),baseColor,metallic);vec3 F=f0+(1-f0)*pow(1-vh,5);return ((1-F)*(1-metallic)*baseColor/3.141592653589793+D*G*F/max(4*nv*nl,.0001))*E*nl;}
void main(){vec3 q=texture(normalMap,uv).rgb*2-1;q.xy*=normalScale;q=normalize(q);vec3 N=legacy==1?normalize(t*q.x+b*q.y+n*q.z):normalize(normalize(t)*q.x+normalize(b)*q.y+normalize(n)*q.z);vec3 V=normalize(vec3(0,0,2)-world);vec3 L=normalize(vec3(-.4,.8,.7));vec3 color;
if(legacy==1){if(dot(N,V)<0)N=-N;float high=1*(1-toony)+shift*toony;float f=clamp((dot(N,L)-shift)/max(.00001,high-shift),0,1);color=min(mix(shadeColor,baseColor,f)+.175*baseColor,baseColor);}
else{if(!gl_FrontFacing)N=-N;color=brdf(N,V,L,vec3(3.8))+brdf(N,V,normalize(vec3(.7,.2,-.4)),vec3(.65,.8,1.0))+.045*baseColor;color=color/(color+1);}
result=vec4(srgb(color),1);})GLSL";
  auto v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);glDeleteShader(v);glDeleteShader(f);if(!ok)throw std::runtime_error("Independent consumer shader link failed");glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glGenBuffers(1,&ibo);
 }
 ~Renderer(){glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ibo);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);}
 void render(cgltf_data *data,const nlohmann::json *vrm=nullptr,Quat source_to_native={}) {
  vertices=triangles=0;glViewport(0,0,96,96);glDisable(GL_FRAMEBUFFER_SRGB);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CCW);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glUseProgram(program);glUniform1i(glGetUniformLocation(program,"normalMap"),0);glUniform1i(glGetUniformLocation(program,"legacy"),vrm?1:0);
  std::map<const cgltf_skin*,std::pair<std::vector<Matrix>,std::vector<NormalMatrix>>> rigs;
  for(size_t node_index=0;node_index<data->nodes_count;++node_index){auto &node=data->nodes[node_index];if(!node.mesh)continue;
   if(node.skin && !rigs.contains(node.skin)){std::pair<std::vector<Matrix>,std::vector<NormalMatrix>> rig;for(size_t j=0;j<node.skin->joints_count;++j){Matrix world{},bind{};cgltf_node_transform_world(node.skin->joints[j],world.data());if(!cgltf_accessor_read_float(node.skin->inverse_bind_matrices,j,bind.data(),16))throw std::runtime_error("Consumer IBM missing");auto m=multiply(world,bind);rig.first.push_back(m);rig.second.push_back(normal_matrix(m));}rigs.emplace(node.skin,std::move(rig));}
   Matrix node_world{};cgltf_node_transform_world(&node,node_world.data());auto node_normal=normal_matrix(node_world);
   for(size_t primitive_index=0;primitive_index<node.mesh->primitives_count;++primitive_index){auto &p=node.mesh->primitives[primitive_index];auto positions=attr(p.attributes,p.attributes_count,cgltf_attribute_type_position),normals=attr(p.attributes,p.attributes_count,cgltf_attribute_type_normal),tangents=attr(p.attributes,p.attributes_count,cgltf_attribute_type_tangent),uvs=attr(p.attributes,p.attributes_count,cgltf_attribute_type_texcoord);if(!positions||!normals||!tangents||!uvs||!p.material)throw std::runtime_error("Consumer required attributes missing");
    struct Vertex {Vec3 p,n;std::array<float,4>t;std::array<float,2>uv;};std::vector<Vertex> values;values.reserve(positions->count);
    for(size_t v=0;v<positions->count;++v){auto P=vec(positions,v),N=vec(normals,v),T=vec(tangents,v);float tangent[4]{};cgltf_accessor_read_float(tangents,v,tangent,4);
     for(size_t target=0;target<p.targets_count;++target){float w=target<node.mesh->weights_count?node.mesh->weights[target]:0;if(w==0)continue;const auto &m=p.targets[target];if(auto a=attr(m.attributes,m.attributes_count,cgltf_attribute_type_position))P+=vec(a,v)*w;if(auto a=attr(m.attributes,m.attributes_count,cgltf_attribute_type_normal))N+=vec(a,v)*w;if(auto a=attr(m.attributes,m.attributes_count,cgltf_attribute_type_tangent))T+=vec(a,v)*w;}
     if(node.skin){Vec3 wp{},wn{},wt{};auto &rig=rigs.at(node.skin);for(int set=0;;++set){auto joints=attr(p.attributes,p.attributes_count,cgltf_attribute_type_joints,set),weights=attr(p.attributes,p.attributes_count,cgltf_attribute_type_weights,set);if(!joints)break;float ji[4]{},w[4]{};if(!weights||!cgltf_accessor_read_float(joints,v,ji,4)||!cgltf_accessor_read_float(weights,v,w,4))throw std::runtime_error("Consumer influences missing");for(size_t k=0;k<4;++k)if(w[k]!=0){auto slot=size_t(ji[k]);wp+=point(rig.first.at(slot),P)*w[k];wn+=direction(rig.second.at(slot),N)*w[k];wt+=direction(rig.first.at(slot),T)*w[k];}}P=wp;N=wn;T=wt;}
     else{P=point(node_world,P);N=direction(node_normal,N);T=direction(node_world,T);}
     P=source_to_native.rotate(P);N=source_to_native.rotate(N);T=source_to_native.rotate(T);float uv[2]{};cgltf_accessor_read_float(uvs,v,uv,2);values.push_back({P,N,{T.x,T.y,T.z,tangent[3]},{uv[0],uv[1]}});
    }
    const cgltf_texture *normal_texture=nullptr;float normal_scale=1;Vec3 base{},shade{};float shift=0,toony=0;
    if(vrm){size_t material=size_t(p.material-data->materials);const auto &m=vrm->at("materialProperties").at(material);const auto &c=m.at("vectorProperties").at("_Color"),&s=m.at("vectorProperties").at("_ShadeColor");base={linear(c[0]),linear(c[1]),linear(c[2])};shade={linear(s[0]),linear(s[1]),linear(s[2])};shift=m.at("floatProperties").at("_ShadeShift");toony=m.at("floatProperties").at("_ShadeToony");normal_scale=m.at("floatProperties").at("_BumpScale");int texture=m.at("textureProperties").at("_BumpMap");normal_texture=&data->textures[texture];if(m.at("floatProperties").at("_IndirectLightIntensity")!=0)throw std::runtime_error("Consumer fixture requires declared uniform GI");}
    else{const auto &m=p.material->pbr_metallic_roughness;base={m.base_color_factor[0],m.base_color_factor[1],m.base_color_factor[2]};normal_texture=p.material->normal_texture.texture;normal_scale=p.material->normal_texture.scale;glUniform1f(glGetUniformLocation(program,"roughness"),m.roughness_factor);glUniform1f(glGetUniformLocation(program,"metallic"),m.metallic_factor);}
    glUniform3f(glGetUniformLocation(program,"baseColor"),base.x,base.y,base.z);glUniform3f(glGetUniformLocation(program,"shadeColor"),shade.x,shade.y,shade.z);glUniform1f(glGetUniformLocation(program,"shift"),shift);glUniform1f(glGetUniformLocation(program,"toony"),toony);glUniform1f(glGetUniformLocation(program,"normalScale"),normal_scale);
    auto pixels=image(normal_texture);GLuint texture;glGenTextures(1,&texture);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glBindSampler(0,0);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,pixels.width,pixels.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.rgba.data());auto sampler=normal_texture->sampler;int min=sampler&&sampler->min_filter?int(sampler->min_filter):GL_LINEAR;glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,min);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,sampler&&sampler->mag_filter?int(sampler->mag_filter):GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,sampler?int(sampler->wrap_s):GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,sampler?int(sampler->wrap_t):GL_REPEAT);if(min>=GL_NEAREST_MIPMAP_NEAREST&&min<=GL_LINEAR_MIPMAP_LINEAR)glGenerateMipmap(GL_TEXTURE_2D);
    std::vector<uint32_t> indices;for(size_t i=0;i<p.indices->count;++i)indices.push_back(uint32_t(cgltf_accessor_read_index(p.indices,i)));glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,values.size()*sizeof(Vertex),values.data(),GL_STATIC_DRAW);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(uint32_t),indices.data(),GL_STATIC_DRAW);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,p)));glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,n)));glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,t)));glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));for(int i=0;i<4;++i)glEnableVertexAttribArray(i);glDrawElements(GL_TRIANGLES,GLsizei(indices.size()),GL_UNSIGNED_INT,nullptr);glDeleteTextures(1,&texture);vertices+=values.size();triangles+=indices.size()/3;
   }
  }
  if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Independent parsed-file consumer GL failure");
 }
};
struct Model {cgltf_data *data=nullptr;nlohmann::json json;explicit Model(const std::filesystem::path &path){cgltf_options options{};if(cgltf_parse_file(&options,path.c_str(),&data)!=cgltf_result_success||cgltf_load_buffers(&options,data,path.c_str())!=cgltf_result_success||cgltf_validate(data)!=cgltf_result_success)throw std::runtime_error("Independent consumer cannot parse/validate exported model");std::ifstream file(path,std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(file)),{});uint32_t size=uint32_t(b[12])|uint32_t(b[13])<<8|uint32_t(b[14])<<16|uint32_t(b[15])<<24;json=nlohmann::json::parse(b.begin()+20,b.begin()+20+size);}~Model(){cgltf_free(data);}Model(const Model&)=delete;};
} // namespace frame_consumer
