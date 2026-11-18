// SPDX-License-Identifier: GPL-3.0-or-later
// Observation only: forward the original read unchanged; query current context
// and immutable driver strings afterward. Never select/create/mutate a context.
#include <GL/gl.h>
#include <EGL/egl.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <unistd.h>
namespace {
std::atomic<unsigned long> sequence{0};
template<class T>T symbol(const char*name,void*scope){auto result=reinterpret_cast<T>(dlsym(scope,name));if(!result){const char msg[]="HDR_ARC_CONTEXT_ERROR missing function\n";write(STDERR_FILENO,msg,sizeof(msg)-1);_exit(86);}return result;}
}
extern "C" void GLAPIENTRY glReadPixels(GLint x,GLint y,GLsizei width,GLsizei height,GLenum format,GLenum type,void*pixels){
 using Read=void(GLAPIENTRY*)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*);
 using String=const GLubyte*(GLAPIENTRY*)(GLenum);
 using Context=EGLContext(EGLAPIENTRYP)();using Display=EGLDisplay(EGLAPIENTRYP)();
 static const auto read=symbol<Read>("glReadPixels",RTLD_NEXT);
 static const auto string=symbol<String>("glGetString",RTLD_DEFAULT);
 static const auto context=symbol<Context>("eglGetCurrentContext",RTLD_DEFAULT);
 static const auto display=symbol<Display>("eglGetCurrentDisplay",RTLD_DEFAULT);
 read(x,y,width,height,format,type,pixels);
 const auto actual_context=context();const auto actual_display=display();
 const auto vendor=reinterpret_cast<const char*>(string(GL_VENDOR));
 const auto renderer=reinterpret_cast<const char*>(string(GL_RENDERER));
 char line[2048];const int count=std::snprintf(line,sizeof(line),"HDR_ARC_CONTEXT read=%lu context=%p display=%p width=%d height=%d format=%u type=%u vendor=%s renderer=%s\n",sequence.fetch_add(1,std::memory_order_relaxed)+1,static_cast<void*>(actual_context),static_cast<void*>(actual_display),width,height,format,type,vendor?vendor:"(null)",renderer?renderer:"(null)");
 if(count<=0||static_cast<size_t>(count)>=sizeof(line)){const char msg[]="HDR_ARC_CONTEXT_ERROR report overflow\n";write(STDERR_FILENO,msg,sizeof(msg)-1);_exit(87);}
 write(STDERR_FILENO,line,static_cast<size_t>(count));
}
