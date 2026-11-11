// SPDX-License-Identifier: GPL-3.0-or-later
// Altered ArmorPaint629f8 texture-space cross blur. Exact weight function is
// generated from pinned vendor/make_blur.c; retain its zlib/libpng notice.
// Native additions: chart filtering, separate authored alpha and coverage, PBR
// color, independent layer mask, 32px dirty tiles/7px halo, explicit worker GPU
// ownership and asynchronous bounded durable result transfer. No packed roles.
#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>
#include "paint-blur-gpu.hpp"
#include "blur_weights.hpp"
#include "graphics.h"
#include <EGL/eglext.h>
#include <chrono>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <thread>
namespace dome::detail {
namespace {
void stop(const std::function<bool()>&cancelled){if(cancelled&&cancelled())throw std::runtime_error("GPU layer blur cancelled");}
GLuint shader(GLenum kind,const char*source){
 GLuint n=glCreateShader(kind);if(!n)throw std::runtime_error("Cannot allocate GPU blur shader");
 glShaderSource(n,1,&source,nullptr);glCompileShader(n);GLint ok=0;glGetShaderiv(n,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[4096]{};glGetShaderInfoLog(n,sizeof(log),nullptr,log);glDeleteShader(n);throw std::runtime_error(log);}return n;
}
}
struct ColorBlurGpu::Impl {
 NativeGraphics graphics;
 std::string device;
 GLuint program=0,vao=0,fbo=0,pbo=0;std::array<GLuint,3>source{},output{};
 GLsync fence=nullptr;
 ~Impl(){
  if(fence)glDeleteSync(fence);
  glDeleteBuffers(1,&pbo);glDeleteTextures(3,source.data());glDeleteTextures(3,output.data());
  glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);
 }
};
ColorBlurGpu::ColorBlurGpu(bool allow_software_validation):impl_(std::make_unique<Impl>()){
 auto&p=*impl_;
 const char*renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 if(!renderer)throw std::runtime_error("GPU blur requires a current native OpenGL context");
 p.device=renderer;
 GLint major=0,minor=0,draw_buffers=0,attachments=0,max_texture=0;
 glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);glGetIntegerv(GL_MAX_DRAW_BUFFERS,&draw_buffers);glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS,&attachments);glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max_texture);
 if(major<3||(major==3&&minor<3)||draw_buffers<3||attachments<3||max_texture<int(blur_input))throw std::runtime_error("GPU layer blur requires OpenGL3.3 float-MRT tile capabilities");
 const bool software=p.device.find("llvmpipe")!=std::string::npos||p.device.find("softpipe")!=std::string::npos;
 auto display_query=reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));
 auto device_query=reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
 EGLAttrib owner=0;const char*node=nullptr;
 if(display_query&&device_query&&display_query(p.graphics.display,EGL_DEVICE_EXT,&owner))node=device_query(reinterpret_cast<EGLDeviceEXT>(owner),EGL_DRM_RENDER_NODE_FILE_EXT);
 if(!allow_software_validation&&(software||!node||!*node||!std::filesystem::exists(node)))throw std::runtime_error("GPU layer blur requires an authenticated hardware EGLDevice/DRM render node; no CPU fallback");
 if(node&&*node)p.device+=" | "+std::string(node);
 const char*vertex="#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}";
 std::string fragment="#version 330 core\nuniform sampler2D commonColor,pbrColor,metadata;uniform float strength;layout(location=0)out vec4 commonResult;layout(location=1)out vec4 pbrResult;layout(location=2)out vec4 alphaResult;\n";
 fragment+=armorpaint_blur_weights;
 fragment+=R"shader(
vec3 decode_srgb(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),greaterThan(c,vec3(.04045)));}
vec3 encode_srgb(vec3 c){c=max(c,vec3(0));return mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-.055,greaterThan(c,vec3(.0031308)));}
void main(){
 ivec2 p=ivec2(gl_FragCoord.xy)+ivec2(7);vec4 meta=texelFetch(metadata,p,0);
 vec4 old0=texelFetch(commonColor,p,0),old1=texelFetch(pbrColor,p,0);
 vec4 sum0=vec4(0),sum1=vec4(0);vec2 alpha=vec2(0);float weight=0;
 for(int axis=0;axis<2;++axis)for(int i=0;i<15;++i){
  ivec2 q=p+(axis==0?ivec2(i-7,0):ivec2(0,i-7));vec4 m=texelFetch(metadata,q,0);
  if(meta.w==0.0||m.w!=meta.w)continue;
  vec4 c0=texelFetch(commonColor,q,0),c1=texelFetch(pbrColor,q,0);float w=get_blur_tool_weight(i);
  sum0+=vec4(decode_srgb(c0.rgb)*c0.a,c0.a)*w;sum1+=vec4(decode_srgb(c1.rgb)*c1.a,c1.a)*w;
  alpha+=m.xy*c0.a*w;weight+=w;
 }
 vec4 associated0=vec4(decode_srgb(old0.rgb)*old0.a,old0.a),associated1=vec4(decode_srgb(old1.rgb)*old1.a,old1.a);
 float gate=meta.w==0.0?0.0:meta.z*strength;
 commonResult=associated0;pbrResult=associated1;alphaResult=vec4(meta.xy*old0.a,0.0,old0.a);
 if(weight>0.0){commonResult=mix(associated0,sum0/weight,gate);pbrResult=mix(associated1,sum1/weight,gate);alphaResult.xy=mix(meta.xy*old0.a,alpha/weight,gate);alphaResult.w=commonResult.a;}
 if(commonResult.a>0.0){commonResult.rgb=encode_srgb(commonResult.rgb/commonResult.a);pbrResult.rgb=encode_srgb(pbrResult.rgb/pbrResult.a);alphaResult.xy/=commonResult.a;}
 else{commonResult.rgb=old0.rgb;pbrResult.rgb=old1.rgb;alphaResult.xy=meta.xy;}
})shader";
 GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=0;
 try{fs=shader(GL_FRAGMENT_SHADER,fragment.c_str());p.program=glCreateProgram();if(!p.program)throw std::runtime_error("Cannot allocate GPU blur program");glAttachShader(p.program,vs);glAttachShader(p.program,fs);glLinkProgram(p.program);GLint ok=0;glGetProgramiv(p.program,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Cannot link native GPU blur program");}
 catch(...){glDeleteShader(vs);if(fs)glDeleteShader(fs);throw;}
 glDeleteShader(vs);glDeleteShader(fs);
 glGenVertexArrays(1,&p.vao);glGenFramebuffers(1,&p.fbo);glGenBuffers(1,&p.pbo);
 glGenTextures(3,p.source.data());glGenTextures(3,p.output.data());
 if(!p.vao||!p.fbo||!p.pbo)throw std::runtime_error("Cannot allocate GPU blur ownership objects");
 glBindFramebuffer(GL_FRAMEBUFFER,p.fbo);
 for(size_t i=0;i<3;++i){
  if(!p.source[i]||!p.output[i])throw std::runtime_error("Cannot allocate GPU blur tile textures");
  for(int which=0;which<2;++which){GLuint texture=which?p.output[i]:p.source[i];GLsizei size=which?blur_tile:blur_input;glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,size,size,0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,p.output[i],0);
 }
 const GLenum buffers[]{GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2};glDrawBuffers(3,buffers);
 if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("GPU blur float tile framebuffer incomplete");
 glBindBuffer(GL_PIXEL_PACK_BUFFER,p.pbo);glBufferData(GL_PIXEL_PACK_BUFFER,sizeof(ColorBlurOutput),nullptr,GL_STREAM_READ);
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Cannot allocate bounded GPU blur tile storage");
}
ColorBlurGpu::~ColorBlurGpu()=default;
const std::string&ColorBlurGpu::device()const noexcept{return impl_->device;}
void ColorBlurGpu::run(const ColorBlurInput&input,ColorBlurOutput&output,float strength,const std::function<bool()>&cancelled){
 if(!std::isfinite(strength)||strength<0||strength>1)throw std::invalid_argument("Invalid native GPU blur strength");
 stop(cancelled);auto&p=*impl_;
 for(size_t i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,p.source[i]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,blur_input,blur_input,GL_RGBA,GL_FLOAT,i<2?static_cast<const void*>(input.color[i].data()):static_cast<const void*>(input.metadata.data()));}
 glUseProgram(p.program);glUniform1i(glGetUniformLocation(p.program,"commonColor"),0);glUniform1i(glGetUniformLocation(p.program,"pbrColor"),1);glUniform1i(glGetUniformLocation(p.program,"metadata"),2);glUniform1f(glGetUniformLocation(p.program,"strength"),strength);
 glBindVertexArray(p.vao);glBindFramebuffer(GL_FRAMEBUFFER,p.fbo);glViewport(0,0,blur_tile,blur_tile);glDrawArrays(GL_TRIANGLES,0,3);
 glBindBuffer(GL_PIXEL_PACK_BUFFER,p.pbo);glPixelStorei(GL_PACK_ALIGNMENT,1);
 for(size_t i=0;i<3;++i){glReadBuffer(GL_COLOR_ATTACHMENT0+i);glReadPixels(0,0,blur_tile,blur_tile,GL_RGBA,GL_FLOAT,reinterpret_cast<void*>(i*sizeof(BlurOutputPlane)));}
 p.fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);if(!p.fence||glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU blur dispatch/durable transfer failed");
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 for(;;){stop(cancelled);auto result=glClientWaitSync(p.fence,GL_SYNC_FLUSH_COMMANDS_BIT,0);if(result==GL_ALREADY_SIGNALED||result==GL_CONDITION_SATISFIED)break;if(result==GL_WAIT_FAILED||std::chrono::steady_clock::now()>deadline)throw std::runtime_error("GPU blur result fence failed or timed out");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 glDeleteSync(p.fence);p.fence=nullptr;stop(cancelled);
 auto*data=glMapBufferRange(GL_PIXEL_PACK_BUFFER,0,sizeof(ColorBlurOutput),GL_MAP_READ_BIT);if(!data)throw std::runtime_error("Cannot map completed GPU blur changed tile");
 std::memcpy(&output,data,sizeof(output));if(!glUnmapBuffer(GL_PIXEL_PACK_BUFFER))throw std::runtime_error("GPU blur changed tile mapping became invalid");
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU blur changed tile read failed");
 stop(cancelled);
}
}
