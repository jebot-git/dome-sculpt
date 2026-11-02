// SPDX-License-Identifier: GPL-3.0-or-later
#include "xatlas.h"
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
namespace {
std::atomic_bool armed{true},arrived{false},released{false};
std::condition_variable *condition=nullptr;
pthread_mutex_t *predicate=nullptr;
thread_local bool destroying=false;
}
extern "C" void __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *,std::unique_lock<std::mutex> &);
extern "C" void __wrap__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *cv,std::unique_lock<std::mutex> &lock) {
  if(armed.exchange(false)) {
    condition=cv;predicate=lock.mutex()->native_handle();
    arrived.store(true);arrived.notify_all();
    // Stop after the predicate's false test while its real mutex is held.
    // No notification can be registered with the platform wait yet.
    while(!released.load())released.wait(false);
  }
  __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(cv,lock);
}
extern "C" void __real__ZNSt18condition_variable10notify_oneEv(std::condition_variable *);
extern "C" void __wrap__ZNSt18condition_variable10notify_oneEv(std::condition_variable *cv) {
  if(destroying&&cv==condition&&!released.load()) {
    std::fputs("Atlas shutdown notifies without acquiring the worker predicate mutex\n",stderr);
    _exit(3);
  }
  __real__ZNSt18condition_variable10notify_oneEv(cv);
}
extern "C" int __real_pthread_mutex_lock(pthread_mutex_t *);
extern "C" int __wrap_pthread_mutex_lock(pthread_mutex_t *mutex) {
  if(destroying&&mutex==predicate){released.store(true);released.notify_all();}
  return __real_pthread_mutex_lock(mutex);
}
int child() {
  auto atlas=xatlas::Create();if(!atlas)return 4;
  while(!arrived.load())arrived.wait(false);
  destroying=true;xatlas::Destroy(atlas);destroying=false;
  if(!released.load())return 5;
  return 0;
}
int main() {
  for(int repeat=0;repeat<32;++repeat) {
    auto pid=fork();if(pid<0)return 6;
    if(!pid)return child();
    int status=0;if(waitpid(pid,&status,0)!=pid)return 7;
    if(!WIFEXITED(status)||WEXITSTATUS(status))return WIFEXITED(status)?WEXITSTATUS(status):8;
  }
  std::puts("PASS 32 deterministic Atlas shutdown predicate/wait-registration handshakes");return 0;
}
