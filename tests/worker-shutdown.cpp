// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <array>
#include <atomic>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
namespace {
std::atomic<bool> armed{false};
std::array<std::atomic<bool>,5> released{};
std::atomic<unsigned> slots{0}, arrived{0};
std::array<std::condition_variable *,5> conditions{};
std::array<pthread_mutex_t *,5> mutexes{};
thread_local bool deleting_document=false;

}
extern "C" void __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *,std::unique_lock<std::mutex> &);
extern "C" void __wrap__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *condition,std::unique_lock<std::mutex> &lock) {
  if(armed.load()) {
    auto slot=slots.fetch_add(1);
    if(slot<conditions.size()) {
      conditions[slot]=condition;mutexes[slot]=lock.mutex()->native_handle();
      arrived.fetch_add(1);arrived.notify_all();
      // Hold the actual predicate mutex after its false check, immediately
      // before registering with the platform condition variable.
      while(!released[slot].load())released[slot].wait(false);
    }
  }
  __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(condition,lock);
}
extern "C" void __real__ZNSt18condition_variable10notify_allEv(std::condition_variable *);
extern "C" void __wrap__ZNSt18condition_variable10notify_allEv(std::condition_variable *condition) {
  if(deleting_document)
    for(size_t i=0;i<conditions.size();++i)if(conditions[i]==condition && !released[i].load()) {
      std::cerr << "Shutdown notified a worker before synchronizing its predicate mutex\n";
      _exit(3);
    }
  __real__ZNSt18condition_variable10notify_allEv(condition);
}
extern "C" int __real_pthread_mutex_lock(pthread_mutex_t *);
extern "C" int __wrap_pthread_mutex_lock(pthread_mutex_t *mutex) {
  if(deleting_document)
    for(size_t i=0;i<mutexes.size();++i)if(mutexes[i]==mutex) {
      released[i]=true;released[i].notify_all();break;
    }
  return __real_pthread_mutex_lock(mutex);
}
int child() {
  armed=true;
  auto document=new dome::Document;
  for(unsigned value=arrived.load();value<conditions.size();value=arrived.load())arrived.wait(value);
  // Release each paused waiter only when shutdown requests that waiter's own
  // predicate mutex. The writer then blocks until platform wait registration.
  // This checks every worker, including the dedicated retopology worker.
  armed=false;deleting_document=true;
  delete document;
  deleting_document=false;
  for(auto &value:released)if(!value.load())return 4;
  return 0;
}
int main() {
  auto process=fork();
  if(process<0)return 2;
  if(!process)_exit(child());
  int status=0;
  auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(std::chrono::steady_clock::now()<end) {
    auto result=waitpid(process,&status,WNOHANG);
    if(result==process) {
      if(WIFEXITED(status)&&WEXITSTATUS(status)==0) {
        std::cout<<"PASS Worker shutdown cannot lose notifications between predicate checks and condition waits\n";
        return 0;
      }
      std::cerr<<"Worker shutdown child failed\n";return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  kill(process,SIGKILL);waitpid(process,&status,0);
  std::cerr<<"FAIL Worker shutdown lost a notification and left a worker blocked\n";
  return 1;
}
