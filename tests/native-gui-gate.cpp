// SPDX-License-Identifier: GPL-3.0-or-later
// Preload-only deterministic preparation barrier for the actual FLTK binary.
// No application test hook: gate one malloc on its diagnostic worker thread.
#include <atomic>
#include <cstdlib>
extern "C" void *__libc_malloc(unsigned long);
#include <fcntl.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
namespace {std::atomic_bool claimed{false};thread_local bool inside=false;}
extern "C" void *malloc(size_t n){
  if(!inside){inside=true;auto root=getenv("DOME_NATIVE_GATE");char name[16]{};
    if(root&&!claimed.load()&&pthread_getname_np(pthread_self(),name,sizeof(name))==0&&name[0]=='d'&&name[5]=='n'&&access(root,F_OK)==0&&!claimed.exchange(true)){
      char marker[4096],release[4096];size_t k=0;for(;root[k]&&k<4000;++k)marker[k]=release[k]=root[k];
      const char hit[]=".hit",done[]=".release";for(unsigned i=0;i<sizeof(hit);++i)marker[k+i]=hit[i];for(unsigned i=0;i<sizeof(done);++i)release[k+i]=done[i];
      int fd=int(syscall(SYS_openat,AT_FDCWD,marker,O_WRONLY|O_CREAT|O_TRUNC,0600));if(fd>=0){syscall(SYS_write,fd,"worker\n",7);syscall(SYS_close,fd);}
      for(unsigned i=0;i<2000&&syscall(SYS_faccessat,AT_FDCWD,release,F_OK)!=0;++i){timespec pause{0,10'000'000};syscall(SYS_nanosleep,&pause,nullptr);}
      if(getenv("DOME_NATIVE_FAIL")){inside=false;return nullptr;}
    }
    inside=false;
  }
  return __libc_malloc(n);
}
