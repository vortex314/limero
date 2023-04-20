#include "limero.h"
#include <algorithm>

Thread::Thread(const char *name) : Named(name)
{
  _priority = tskIDLE_PRIORITY + 1;
  _queueSize = 20;
  _stackSize = 1024;
}

Thread::Thread(ThreadProperties props)
    : Named(props.name),
      _queueSize(props.queueSize),
      _stackSize(props.stackSize),
      _priority(props.priority) {}

TimerSource &Thread::createTimer(uint32_t interval, bool repeat, bool active, const char *__name)
{
  TimerSource *timer = new TimerSource(interval, repeat, active, __name);
  _timers.push_back(timer);
  return *timer;
}
void Thread::deleteTimer(TimerSource &ts)
{
  auto pos = std::find(_timers.begin(), _timers.end(), &ts);
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

int Thread::queue(Invoker *invoker)
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
int Thread::queueFromISR(Invoker *invoker)
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
      uint32_t min_wait = 1; // cycle fast on lock
      // check all timers
      for (TimerSource *timer : _timers)
      {
        if (timer->isExpired())
          timer->invoke();
      }
      min_wait = minWait();

    Invoker *invoker;

    TickType_t tickWaits = pdMS_TO_TICKS(min_wait);
    INFO("Thread '%s' tickWaits %d", name(), min_wait);
    while (xQueueReceive(_workQueue, &invoker, tickWaits) == pdPASS)
    {
      invoker->invoke();
    }
  }
}
