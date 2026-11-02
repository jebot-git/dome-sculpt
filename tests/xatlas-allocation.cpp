// SPDX-License-Identifier: GPL-3.0-or-later
#include "xatlas.h"
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <mutex>
#include <new>
namespace {
thread_local bool deny=false,hit=false;
thread_local size_t count=0,boundary=SIZE_MAX;
std::atomic_bool waiter{false},observe{false};
size_t threads(){size_t result=0;auto dir=opendir("/proc/self/task");if(!dir)throw std::runtime_error("Cannot inspect actual native worker lifetime");while(auto row=readdir(dir))if(row->d_name[0]!='.')++result;closedir(dir);return result;}
bool joined_threads(size_t expected){
  // pthread_join observes the thread's cleared child-TID before /proc must
  // have removed its final kernel task entry. Wait for that actual condition;
  // time is a failure bound only, never evidence of successful cleanup.
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(1);
  do{if(threads()==expected)return true;std::this_thread::yield();}while(std::chrono::steady_clock::now()<end);
  return false;
}
}
extern "C" void __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *,std::unique_lock<std::mutex> &);
extern "C" void __wrap__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *condition,std::unique_lock<std::mutex> &lock) {
  if(observe.load()){waiter=true;waiter.notify_all();}
  __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(condition,lock);
}
[[gnu::noinline]] void *operator new(size_t n) {
  if(deny&&++count>=boundary){
    // Once a prior thread exists, observe its actual entry into wait before
    // denying the next thread-state allocation. No sleep or thread-start race.
    if(boundary>1)while(!waiter.load())waiter.wait(false);
    hit=true;throw std::bad_alloc();
  }
  if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,size_t)noexcept{std::free(p);}
int main(int argc,char **argv)try {
  auto baseline=threads();deny=true;auto atlas=xatlas::Create();deny=false;xatlas::Destroy(atlas);const auto measured=count;
  if(!measured)throw std::runtime_error("No real C++ native thread allocations observed");
  size_t first=1,last=measured;if(argc==2)first=last=std::strtoull(argv[1],nullptr,10);
  for(auto at=first;at<=last;++at){
    waiter=false;observe=true;count=0;boundary=at;hit=false;deny=true;bool caught=false;
    try{atlas=xatlas::Create();}catch(const std::bad_alloc &){caught=true;}
    deny=false;observe=false;
    if(!hit||!caught)throw std::runtime_error("Every measured constructor failure must actually be reached and unwind");
    if(!joined_threads(baseline))throw std::runtime_error("Failed Atlas construction left a live native worker");
    atlas=xatlas::Create();xatlas::Destroy(atlas);
    if(!joined_threads(baseline))throw std::runtime_error("Retry did not join every native worker");
  }
  std::printf("PASS %zu persistent thread-state allocation failures unwind all native workers and allow retry\n",last-first+1);return 0;
}catch(const std::exception &e){deny=false;std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
