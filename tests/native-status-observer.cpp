// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only observation of the real FLTK status label. No application hooks or
// replacement event path: forward every label to the canonical shared toolkit.
#include <cstdlib>
#include <cstring>
#include <string>
#include <GL/gl.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
namespace {
constexpr char expected[] =
    "Sculpt ray exceeds the bounded grid traversal budget at this resolution";
}
extern "C" void
observe_label(void *, const char *) asm("_ZN9Fl_Widget10copy_labelEPKc");
extern "C" void observe_label(void *widget, const char *label) {
  using CopyLabel = void (*)(void *, const char *);
  static const auto original = reinterpret_cast<CopyLabel>(
      dlsym(RTLD_NEXT, "_ZN9Fl_Widget10copy_labelEPKc"));
  if (!original)
    std::abort();
  original(widget, label);
  const char *path = std::getenv("DOME_SCULPT_STATUS_ACK");
  if (!path || !label)return;
  // Keep the last REAL forwarded label for readiness/failure diagnosis; the
  // success marker below still requires the precise candidate-budget error.
  if(std::strncmp(label,"Dome Sculpt",11)!=0){
  const auto latest=std::string(path)+".latest";
  const int trace=open(latest.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
  if(trace>=0){const auto ignored=write(trace,label,strnlen(label,2048));(void)ignored;close(trace);}
  }
  if (!std::strstr(label, expected))return;
  const int fd =
      open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (fd < 0)
    return;
  const auto ignored = write(fd, label, strnlen(label, 2048));
  (void)ignored;
  close(fd);
}

// The asynchronous desktop path may expose loaded menus before its immutable
// render surface is ready. A genuine indexed draw of the fixture's exact index
// domain acknowledges readiness without an arbitrary delay or application API.
extern "C" void observe_draw(GLenum,GLsizei,GLenum,const void*) asm("glDrawElements");
extern "C" void observe_draw(GLenum mode,GLsizei count,GLenum type,const void *indices){
  using Draw=void(*)(GLenum,GLsizei,GLenum,const void*);
  static const auto original=reinterpret_cast<Draw>(dlsym(RTLD_NEXT,"glDrawElements"));
  if(!original)std::abort();original(mode,count,type,indices);
  const char *path=std::getenv("DOME_SCULPT_DRAW_ACK"),*wanted=std::getenv("DOME_SCULPT_DRAW_COUNT");
  if(!path||!wanted||mode!=GL_TRIANGLES||count<=0||std::strtoull(wanted,nullptr,10)!=static_cast<unsigned long long>(count))return;
  int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0)return;
  const auto ignored=write(fd,wanted,strnlen(wanted,32));(void)ignored;close(fd);
}
