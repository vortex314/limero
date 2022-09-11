#include "limero.h"

#include "Log.h"

// #include <asm-generic/ioctls.h>
#include <errno.h>
#include <fcntl.h>
// #include <poll.h>
#include <stdint.h>
//#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>

std::unordered_map<void *, std::forward_list<Subscription *> *>
    Subscription::_subscriptionsPerSource;
    LogStack logStack;


// int Thread::_id = 0;
std::vector<Thread *> _threads;

Thread::Thread(const char *name) : Named(name), _workQueue(10) {
  _threads.push_back(this);
};

void Thread::createQueue() {}

void Thread::start() {}

int Thread::enqueue(Invoker *invoker) {
  return _workQueue.push(invoker) == true ? 0 : ENOSPC;
};

int Thread::enqueueFromIsr(Invoker *invoker) {
  return _workQueue.push(invoker) == true ? 0 : ENOSPC;
};

void Thread::addTimer(TimerSource *ts) { _timers.push_back(ts); }

void Thread::run() {
  INFO("Thread '%s' started ", name());
  while (true) loop();
}

void Thread::loop() {
  // execute thread waiters
  Invoker *prq;
  while (_workQueue.pop(prq) == true) {
    prq->invoke();
  }
  // execute expired timers
  for (auto timer : _timers) {
    if (timer->expireTime() < Sys::millis()) {
      timer->request();
      break;
    }
  }
}

void Thread::loopAll() {
  for (auto thr : _threads) thr->loop();
}
