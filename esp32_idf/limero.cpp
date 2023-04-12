#include "limero.h"

#include <algorithm>

std::unordered_map<void *, std::forward_list<Subscription *> *>
    Subscription::_subscriptionsPerSource;

/*
 _____ _                        _
|_   _| |__  _ __ ___  __ _  __| |
  | | | '_ \| '__/ _ \/ _` |/ _` |
  | | | | | | | |  __/ (_| | (_| |
  |_| |_| |_|_|  \___|\__,_|\__,_|
*/
int Thread::_id = 0;

Thread::Thread(const char *name) : Named(name)
{
  _priority = tskIDLE_PRIORITY + 1;
  _queueSize = 20;
}

Thread::Thread(ThreadProperties props)
    : Named(props.name),
      _queueSize(props.queueSize),
      _stackSize(props.stackSize),
      _priority(props.priority) {}

void Thread::addTimer(TimerSource *ts) { _timers.push_back(ts); }

void Thread::delTimer(TimerSource *ts)
{
  auto pos = std::find(_timers.begin(), _timers.end(), ts);
  if (pos != _timers.end())
    _timers.erase(pos);
}

void Thread::createQueue()
{
  _workQueue = xQueueCreate(_queueSize ? _queueSize : 20, sizeof(Invoker *));
  if (_workQueue == NULL)
    WARN("Queue creation failed ");
}

void Thread::start()
{
  xTaskCreate([](void *task)
              { ((Thread *)task)->run(); },
              name(),
              _stackSize ? _stackSize : 10000, this, _priority, NULL);
  /*
      xTaskCreatePinnedToCore([](void* task) {
              ((Thread*)task)->run();
      }, _name.c_str(), 20000, this, 17, NULL, PRO_CPU);*/
}

int Thread::enqueue(Invoker *invoker)
{
  //	INFO("Thread '%s' >>> '%s'",_name.c_str(),symbols(invoker));
  if (_workQueue)
    if (xQueueSend(_workQueue, &invoker, (TickType_t)0) != pdTRUE)
    {
      WARN("Thread '%s' queue overflow [%X]", name(), invoker);
      return ENOBUFS;
    }
  return 0;
};

class DummyInvoker : public Invoker
{
public:
  void invoke(){};
};

DummyInvoker NO_MESSAGE;
DummyInvoker *NO_MESSAGE_PTR = &NO_MESSAGE;

void Thread::wake()
{
  if (_workQueue)
    if (xQueueSend(_workQueue, &NO_MESSAGE_PTR, (TickType_t)0) != pdTRUE)
    {
      WARN("Thread '%s' queue overflow [%X]", name(), NULL);
    }
}
int Thread::enqueueFromIsr(Invoker *invoker)
{
  if (_workQueue)
  {
    if (xQueueSendFromISR(_workQueue, &invoker, (TickType_t)0) != pdTRUE)
    {
      //  WARN("queue overflow"); // cannot log here concurency issue
      return ENOBUFS;
    }
  }
  return 0;
};

void timeExec(const char *name, std::function<void()> f, uint32_t warn)
{
  uint64_t start = Sys::millis();
  f();
  uint32_t delta = Sys::millis() - start;
  if (delta > warn)
    WARN("Execution %s took %d msec", name, delta);
}

void Thread::run()
{
  INFO("Thread '%s' prio : %d started ", name(), uxTaskPriorityGet(NULL));
  createQueue();
  while (true)
  {
    uint64_t now = Sys::millis();
    uint64_t soonestExpiration = now + 5000;
    // find next expired timer if any within 5 sec
    for (auto timer : _timers)
    {
      if (timer->expireTime() != UINT64_MAX)
      {
        if (timer->expireTime() <= now)
        {
 //         INFO(" TimerSource [%X] execution", timer);
          timeExec(
              "Timer request", [timer]()
              { timer->request(); },
              10);
          if (timer->expireTime() == UINT64_MAX)
          {
//            INFO(" TimerSource [%X] is disabled", timer);
            continue;
          }
//          else
//            INFO(" TimerSource [%X] next expiration %llu msec", timer, timer->expireTime() - now);
        }
        if (timer->expireTime() <= soonestExpiration)
        {
          soonestExpiration = timer->expireTime();
//          INFO(" TimerSource [%X] next expiration %llu msec", timer, timer->expireTime() - now);
        }
      }
    }

    Invoker *prq;
    uint32_t wait = soonestExpiration - now;
    TickType_t tickWaits = pdMS_TO_TICKS(wait);
//    INFO("Thread '%s' tickWaits %d", name(), wait);
    if (xQueueReceive(_workQueue, &prq, tickWaits) == pdPASS)
    {
      timeExec(
          "Invoker", [prq]()
          { prq->invoke(); },
          10);
    }
  }
}
