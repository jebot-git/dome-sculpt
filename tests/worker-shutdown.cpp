// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <array>
#include <atomic>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
namespace dome {struct DocumentNativeTestAccess {
  static std::condition_variable *condition(Document &d){return &d.native_changed_;}
};}
namespace {
std::atomic<bool> native_armed{false};std::condition_variable *native_condition=nullptr;
std::atomic<bool> armed{false};
std::array<std::atomic<bool>,7> released{};
std::atomic<unsigned> slots{0}, arrived{0};
std::array<std::condition_variable *,7> conditions{};
std::array<pthread_mutex_t *,7> mutexes{};
thread_local bool deleting_document=false;

}
extern "C" void __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *,std::unique_lock<std::mutex> &);
extern "C" void __wrap__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *condition,std::unique_lock<std::mutex> &lock) {
  if(native_armed.load()&&condition==native_condition&&native_armed.exchange(false)) {
    conditions[0]=condition;mutexes[0]=lock.mutex()->native_handle();arrived=1;arrived.notify_all();
    while(!released[0].load())released[0].wait(false);
  }
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
  // This checks every worker, including the dedicated retopology and cut workers.
  armed=false;deleting_document=true;
  delete document;
  deleting_document=false;
  for(auto &value:released)if(!value.load())return 4;
  return 0;
}
int active_child() {
  auto document=new dome::Document;dome::Volume v(.05f);v.begin_stroke();dome::Brush b;b.radius=.23f;b.blend=0;v.stamp(b);v.end_stroke();document->replace(v.geometry_snapshot());
  if(!document->wait_mesh())return 5;
  native_condition=dome::DocumentNativeTestAccess::condition(*document);native_armed=true;
  if(!document->begin_native_stroke())return 6;
  for(unsigned value=arrived.load();!value;value=arrived.load())arrived.wait(value);
  deleting_document=true;document->end_native_stroke(false);deleting_document=false;
  if(!document->wait_native_stroke())return 7;
  delete document;return 0;
}
int main() {
  for(bool active:{false,true}) {
    auto process=fork();if(process<0)return 2;if(!process)_exit(active?active_child():child());
    int status=0;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);bool finished=false;
    while(std::chrono::steady_clock::now()<end) {
      auto result=waitpid(process,&status,WNOHANG);
      if(result==process){finished=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if(!finished){kill(process,SIGKILL);waitpid(process,&status,0);std::cerr<<"FAIL Worker lost predicate notification\n";return 1;}
    if(!WIFEXITED(status)||WEXITSTATUS(status)!=0){std::cerr<<"Worker protocol child failed (active="<<active<<")\n";return 1;}
  }
  std::cout<<"PASS All seven worker shutdown predicates and active native gesture cancellation synchronize before notification\n";return 0;
}
