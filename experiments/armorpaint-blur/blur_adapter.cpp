// SPDX-License-Identifier: GPL-3.0-or-later
#include "blur_adapter.hpp"
#include "blur_shader.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
namespace armorpaint_proof {
namespace {
GLuint compile(GLenum type,const char* source) {
 GLuint shader=glCreateShader(type);
 if(!shader)throw std::runtime_error("shader allocation failed");
 glShaderSource(shader,1,&source,nullptr);
 glCompileShader(shader); GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
 if(!ok){std::array<char,4096> log{};glGetShaderInfoLog(shader,log.size(),nullptr,log.data());glDeleteShader(shader);throw std::runtime_error(log.data());}
 return shader;
}
// Save actual state used by the adapter. No texture upload, readback, sampling
// parameter mutation, source mutation, or native document access in run().
struct State {
 GLint program,draw,vao,active,viewport[4],texture[2],sampler[2],polygon[2];
 GLboolean switches[7],mask[4];
 static constexpr GLenum caps[]={GL_BLEND,GL_SCISSOR_TEST,GL_DEPTH_TEST,GL_CULL_FACE,GL_RASTERIZER_DISCARD,GL_FRAMEBUFFER_SRGB,GL_STENCIL_TEST};
 State(){
  glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
  glGetIntegerv(GL_VIEWPORT,viewport);glGetBooleanv(GL_COLOR_WRITEMASK,mask);
  glGetIntegerv(GL_POLYGON_MODE,polygon);
  for(int i=0;i<7;++i)switches[i]=glIsEnabled(caps[i]);
  for(int i=0;i<2;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture[i]);glGetIntegeri_v(GL_SAMPLER_BINDING,i,&sampler[i]);}
 }
 ~State(){
  glUseProgram(program);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindVertexArray(vao);
  glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glColorMask(mask[0],mask[1],mask[2],mask[3]);
  glPolygonMode(GL_FRONT_AND_BACK,polygon[0]);
  for(int i=0;i<7;++i){if(switches[i])glEnable(caps[i]);else glDisable(caps[i]);}
  for(int i=0;i<2;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,texture[i]);glBindSampler(i,sampler[i]);}
  glActiveTexture(active);
 }
};
struct TextureInfo {GLint width,height,format;};
TextureInfo info(GLuint t){
 glBindTexture(GL_TEXTURE_2D,t);
 // A wrong-target texture name leaves the preceding binding unchanged. Never
 // inspect that previous owner's dimensions/format as evidence for this name.
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("texture is not a valid 2D owner");
 TextureInfo r{};glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&r.width);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&r.height);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&r.format);
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("texture metadata query failed");
 return r;
}
}
Blur::Blur(){
 const char* vertex="#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}";
 GLuint vs=compile(GL_VERTEX_SHADER,vertex),fs=0;
 try{fs=compile(GL_FRAGMENT_SHADER,blur_shader);program_=glCreateProgram();if(!program_)throw std::runtime_error("program allocation failed");glAttachShader(program_,vs);glAttachShader(program_,fs);glLinkProgram(program_);GLint ok=0;glGetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("blur link failed");glGenFramebuffers(1,&framebuffer_);if(!framebuffer_)throw std::runtime_error("framebuffer allocation failed");glGenVertexArrays(1,&vao_);if(!vao_)throw std::runtime_error("vertex-array allocation failed");if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GL allocation failed");}
 catch(...){glDeleteShader(vs);if(fs)glDeleteShader(fs);if(program_)glDeleteProgram(program_);if(framebuffer_)glDeleteFramebuffers(1,&framebuffer_);if(vao_)glDeleteVertexArrays(1,&vao_);throw;}
 glDeleteShader(vs);glDeleteShader(fs);
}
Blur::~Blur(){glDeleteFramebuffers(1,&framebuffer_);glDeleteVertexArrays(1,&vao_);glDeleteProgram(program_);}
void Blur::run(GLuint source,GLuint mask,GLuint output,float strength,std::size_t byte_budget){
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("caller requires a clean GL error state");
 if(!source||!mask||!output||source==output||mask==output||source==mask||!glIsTexture(source)||!glIsTexture(mask)||!glIsTexture(output))throw std::runtime_error("invalid/aliasing texture owners");
 if(!std::isfinite(strength)||strength<0||strength>1)throw std::runtime_error("invalid strength");
 State restore;glActiveTexture(GL_TEXTURE0);
 auto a=info(source),b=info(mask),c=info(output);
 if(a.width<=0||a.height<=0||a.width>512||a.height>512||a.width!=b.width||a.height!=b.height||a.width!=c.width||a.height!=c.height||a.format!=GL_RGBA32F||b.format!=GL_R32F||c.format!=GL_RGBA32F)throw std::runtime_error("explicit float format/size contract refused");
 if(std::size_t(a.width)*std::size_t(a.height)*36>byte_budget)throw std::runtime_error("caller texture working budget refused");
 glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer_);glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,output,0);glDrawBuffer(GL_COLOR_ATTACHMENT0);
 if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("float framebuffer incomplete");
 for(auto cap:State::caps){glDisable(cap);}
 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
 glViewport(0,0,a.width,a.height);glUseProgram(program_);glBindVertexArray(vao_);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,source);glBindSampler(0,0);
 glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,mask);glBindSampler(1,0);
 glUniform1i(glGetUniformLocation(program_,"sourcePlane"),0);glUniform1i(glGetUniformLocation(program_,"destinationMask"),1);
 glUniform1f(glGetUniformLocation(program_,"strength"),strength);glDrawArrays(GL_TRIANGLES,0,3);
}
}
