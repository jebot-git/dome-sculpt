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
std::atomic<bool> armed{false}, released{false};
std::atomic<unsigned> slots{0}, arrived{0}, notifications{0};
std::atomic<int> checkpoint{0};
std::array<std::condition_variable *,4> conditions{};
std::array<pthread_mutex_t *,4> mutexes{};
thread_local bool deleting_document=false;
void reached(int value) {
  int zero=0;
  if(checkpoint.compare_exchange_strong(zero,value))checkpoint.notify_all();
}
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
      while(!released.load())released.wait(false);
    }
  }
  __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(condition,lock);
}
extern "C" void __real__ZNSt18condition_variable10notify_allEv(std::condition_variable *);
extern "C" void __wrap__ZNSt18condition_variable10notify_allEv(std::condition_variable *condition) {
  if(deleting_document)
    for(auto current:conditions)if(current==condition) {
      if(notifications.fetch_add(1)==2)reached(1);
      break;
    }
  __real__ZNSt18condition_variable10notify_allEv(condition);
}
extern "C" int __real_pthread_mutex_lock(pthread_mutex_t *);
extern "C" int __wrap_pthread_mutex_lock(pthread_mutex_t *mutex) {
  if(deleting_document)
    for(auto current:mutexes)if(current==mutex){reached(2);break;}
  return __real_pthread_mutex_lock(mutex);
}
int child() {
  armed=true;
  auto document=new dome::Document;
  for(unsigned value=arrived.load();value<4;value=arrived.load())arrived.wait(value);
  std::thread destruction([&]{deleting_document=true;delete document;});
  // Old code notifies three waiters before locking any predicate mutex. Fixed
  // code attempts the mutex first. Both schedules rendezvous deterministically;
  // there is no sleep-based attempt to win a race.
  while(!checkpoint.load())checkpoint.wait(0);
  std::cerr << "Shutdown handshake: " << (checkpoint.load()==1 ? "notified before predicate mutex" : "predicate mutex acquired before notification") << '\n';
  armed=false;released=true;released.notify_all();
  destruction.join();
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
