// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <list>
namespace dome {
// One owning GL thread. RGB accumulates premultiplied in linear floating point.
class LinearSceneTarget {
  struct Target {GLuint fbo=0,color=0,depth=0;int width=0,height=0;GLenum format=GL_DEPTH_COMPONENT24;};
  std::list<Target> targets_;
  GLuint program_=0,vao_=0;
  GLint destination_=0,read_=0;
  GLenum destination_encoding_=GL_LINEAR;
  bool pending_=false,copy_depth_=false;
  int width_=0,height_=0;
  Target *active_=nullptr;
  static GLuint compile(GLenum type,const char*text){auto id=glCreateShader(type);glShaderSource(id,1,&text,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];glGetShaderInfoLog(id,sizeof(log),nullptr,log);glDeleteShader(id);throw std::runtime_error(log);}return id;}
  static void release(Target&t,bool gl){if(gl){glDeleteRenderbuffers(1,&t.depth);glDeleteTextures(1,&t.color);glDeleteFramebuffers(1,&t.fbo);}t={};}
  void initialise(){if(program_)return;GLuint v=0,f=0;try{v=compile(GL_VERTEX_SHADER,R"(#version 330 core
out vec2 uv;void main(){vec2 p=vec2((gl_VertexID==1)?3:-1,(gl_VertexID==2)?3:-1);uv=(p+1)*.5;gl_Position=vec4(p,0,1);})");f=compile(GL_FRAGMENT_SHADER,R"(#version 330 core
in vec2 uv;out vec4 color;uniform sampler2D scene;uniform int encode_srgb;
vec3 encode(vec3 c){return mix(12.92*c,1.055*pow(max(c,vec3(0)),vec3(1.0/2.4))-.055,greaterThan(c,vec3(.0031308)));}
void main(){vec4 c=texture(scene,uv);c.rgb=max(c.rgb,vec3(0));color=vec4(encode_srgb==1?encode(c.rgb):c.rgb,c.a);})");program_=glCreateProgram();glAttachShader(program_,v);glAttachShader(program_,f);glLinkProgram(program_);glDeleteShader(v);glDeleteShader(f);v=f=0;GLint ok=0;glGetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Linear scene resolve shader linking failed");glGenVertexArrays(1,&vao_);}catch(...){glDeleteShader(v);glDeleteShader(f);glDeleteProgram(program_);program_=0;throw;}}
public:
  bool pending()const{return pending_;}
  size_t bytes()const{size_t bytes=0;for(const auto&t:targets_)bytes+=size_t(t.width)*t.height*(t.format==GL_DEPTH32F_STENCIL8?16:12);return bytes;}
  void clear(bool gl){if(pending_){if(gl)abort();else{pending_=false;active_=nullptr;}}for(auto&t:targets_)release(t,gl);targets_.clear();if(gl){glDeleteProgram(program_);glDeleteVertexArrays(1,&vao_);}program_=vao_=0;active_=nullptr;}
  void abort(){if(!pending_)return;glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(destination_));glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read_));glDisable(GL_FRAMEBUFFER_SRGB);pending_=false;active_=nullptr;}
  void begin(int width,int height,SurfaceOutputEncoding output){
    if(pending_)throw std::logic_error("Resolve the previous linear scene before starting another view");
    if(width<=0||height<=0||size_t(width)*height>16*1024*1024)throw std::length_error("Linear scene target exceeds its pixel bounds");
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&destination_);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_);GLint samples=0;glGetIntegerv(GL_SAMPLES,&samples);if(samples>1)throw std::invalid_argument("Linear scene resolve requires a single-sample destination");GLint buffer=0;glGetIntegerv(GL_DRAW_BUFFER0,&buffer);GLenum attachment=destination_?GLenum(buffer):(buffer==GL_FRONT||buffer==GL_FRONT_LEFT?GL_FRONT_LEFT:GL_BACK_LEFT);
    GLint encoding=0;glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,attachment,GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,&encoding);if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Cannot verify destination color encoding");destination_encoding_=GLenum(encoding);
    if(output==SurfaceOutputEncoding::Linear&&destination_encoding_!=GL_LINEAR)throw std::invalid_argument("Linear output requires a linear-encoded destination attachment");
    GLenum depth_attachment=destination_?GL_DEPTH_ATTACHMENT:GL_DEPTH;GLint depth=0,stencil=0,type=GL_UNSIGNED_NORMALIZED;
    GLint depth_object=GL_NONE,stencil_object=GL_NONE;
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,depth_attachment,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,&depth_object);
    if(depth_object!=GL_NONE){glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,depth_attachment,GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,&depth);glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,depth_attachment,GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE,&type);}
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,destination_?GL_STENCIL_ATTACHMENT:GL_STENCIL,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,&stencil_object);
    if(stencil_object!=GL_NONE)glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,destination_?GL_STENCIL_ATTACHMENT:GL_STENCIL,GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,&stencil);
    if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Cannot verify destination depth format");
    copy_depth_=depth>0;
    GLenum format=stencil?(depth==32?GL_DEPTH32F_STENCIL8:GL_DEPTH24_STENCIL8):depth<=16&&depth>0?GL_DEPTH_COMPONENT16:depth==32?(type==GL_FLOAT?GL_DEPTH_COMPONENT32F:GL_DEPTH_COMPONENT32):GL_DEPTH_COMPONENT24;
    width_=width;height_=height;pending_=true;
    try{
      initialise();auto found=std::find_if(targets_.begin(),targets_.end(),[&](const auto&t){return t.width==width&&t.height==height&&t.format==format;});
      if(found==targets_.end()){
        size_t pixels=size_t(width)*height;auto total=[&]{size_t n=pixels;for(auto&t:targets_)n+=size_t(t.width)*t.height;return n;};while(!targets_.empty()&&(targets_.size()>=2||total()>32*1024*1024)){release(targets_.back(),true);targets_.pop_back();}
        Target target;target.width=width;target.height=height;target.format=format;
        try{glGenFramebuffers(1,&target.fbo);glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);glGenTextures(1,&target.color);glBindTexture(GL_TEXTURE_2D,target.color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,width,height,0,GL_RGBA,GL_HALF_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target.color,0);glGenRenderbuffers(1,&target.depth);glBindRenderbuffer(GL_RENDERBUFFER,target.depth);glRenderbufferStorage(GL_RENDERBUFFER,format,width,height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,stencil?GL_DEPTH_STENCIL_ATTACHMENT:GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,target.depth);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE||glGetError()!=GL_NO_ERROR)throw std::runtime_error("Linear scene framebuffer allocation failed");targets_.push_front(target);}catch(...){release(target,true);throw;}
      }else targets_.splice(targets_.begin(),targets_,found);
      active_=&targets_.front();glBindFramebuffer(GL_FRAMEBUFFER,active_->fbo);glViewport(0,0,width,height);glDisable(GL_FRAMEBUFFER_SRGB);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
    }catch(...){abort();throw;}
  }
  void resolve(SurfaceOutputEncoding output){
    if(!pending_)return;
    try{
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(destination_));glViewport(0,0,width_,height_);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
      bool automatic=output==SurfaceOutputEncoding::Srgb&&destination_encoding_==GL_SRGB;if(automatic)glEnable(GL_FRAMEBUFFER_SRGB);else glDisable(GL_FRAMEBUFFER_SRGB);
      glUseProgram(program_);glUniform1i(glGetUniformLocation(program_,"scene"),0);glUniform1i(glGetUniformLocation(program_,"encode_srgb"),output==SurfaceOutputEncoding::Srgb&&!automatic);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,active_->color);glBindSampler(0,0);glBindVertexArray(vao_);glDrawArrays(GL_TRIANGLES,0,3);
      glBindVertexArray(0);glUseProgram(0);glDisable(GL_FRAMEBUFFER_SRGB);glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);
      if(copy_depth_){glBindFramebuffer(GL_READ_FRAMEBUFFER,active_->fbo);glBlitFramebuffer(0,0,width_,height_,0,0,width_,height_,GL_DEPTH_BUFFER_BIT,GL_NEAREST);}
      if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Linear scene GPU resolve failed");
      abort();
    }catch(...){abort();throw;}
  }
};
} // namespace dome
