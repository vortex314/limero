#include "limero.h"
NanoStats stats;

void Thread::addTimer(TimerSource *ts) { _timers.push_back(ts); }

std::vector<Thread *> threads;

void Thread::loopAll()
{
  for (auto thr : threads)
    thr->loop();
}

//================================================================ NO RTOS
#ifdef NO_RTOS
int Thread::_id = 0;

Thread::Thread(const char *name) : Named(name), _workQueue(10) { threads.push_back(this); };

void Thread::createQueue() {}

void Thread::start() {}

int Thread::enqueue(Invoker *invoker)
{
  _workQueue.push(invoker);
  return 0;
};

int Thread::enqueueFromIsr(Invoker *invoker)
{
  _workQueue.push(invoker);
  return 0;
}

void Thread::run()
{
  INFO("Thread '%s' started ", name());
  while (true)
    loop();
}

void Thread::loop()
{
  uint64_t now = Sys::millis();
  for (auto timer : _timers)
  {
    if (timer->expireTime() < now)
    {
      timer->request();
    }
  }
  Invoker *prq;
  while (_workQueue.pop(prq) == true)
  {
    prq->invoke();
  }
}
#endif
//===================================================================== FREERTOS
#ifdef FREERTOS
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
  _priority = tskIDLE_PRIORITY + 2;
}

Thread::Thread(ThreadProperties props)
    : Named(props.name), _queueSize(props.queueSize),
      _stackSize(props.stackSize), _priority(props.priority) {}

void Thread::createQueue()
{
  _workQueue = xQueueCreate(_queueSize ? _queueSize : 20, sizeof(Invoker *));
  if (_workQueue == NULL)
    WARN("Queue creation failed ");
}

void Thread::start()
{
  BaseType_t x = xTaskCreate([](void *task)
                             { ((Thread *)task)->run(); },
                             name(),
                             _stackSize ? _stackSize : 3000, this, _priority, NULL);
  if (x != pdPASS)
    ERROR("xTaskCreate() failed.");
  if (x == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
    ERROR("xTaskCreate() failed due to lack of memory ? .");
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
      stats.threadQueueOverflow++;
      WARN("Thread '%s' queue overflow [%X]", name(), invoker);
      return ENOBUFS;
    }
  return 0;
};
int Thread::enqueueFromIsr(Invoker *invoker)
{
  if (_workQueue)
  {
    if (xQueueSendFromISR(_workQueue, &invoker, (TickType_t)0) != pdTRUE)
    {
      //  WARN("queue overflow"); // cannot log here concurency issue
      stats.threadQueueOverflow++;
      return ENOBUFS;
    }
  }
  return 0;
};

void Thread::run()
{
  INFO("Thread '%s' prio : %d started ", name(), uxTaskPriorityGet(NULL));
  try
  {
    createQueue();
    uint32_t noWaits = 0;
    while (true)
    {
      uint64_t now = Sys::millis();
      uint64_t expTime = now + 5000;
      TimerSource *expiredTimer = 0;
      // find next expired timer if any within 5 sec
      for (auto timer : _timers)
      {
        if (timer->expireTime() < expTime)
        {
          expTime = timer->expireTime();
          expiredTimer = timer;
        }
      }
      int32_t waitTime = (expTime - now);
      // ESP_OPEN_RTOS seems to double sleep time ?
      //		INFO(" waitTime : %d ",waitTime);
      if (noWaits % 1000 == 999)
        WARN(" noWaits : %d in thread %s waitTime %d ", noWaits, name(),
             waitTime);
      if (waitTime > 0)
      {
        Invoker *prq;
        TickType_t tickWaits = pdMS_TO_TICKS(waitTime);
        if (tickWaits == 0)
          noWaits++;
        if (xQueueReceive(_workQueue, &prq, tickWaits) == pdPASS)
        {
          uint64_t start = Sys::millis();
          prq->invoke();
          uint32_t delta = Sys::millis() - start;
          if (delta > 50)
            WARN("Invoker [%X] slow %d msec invoker on thread '%s'.", prq,
                 delta, name());
        }
        else
        {
          noWaits = 0;
        }
      }
      else
      {
        noWaits++;
        if (expiredTimer)
        {
          if (-waitTime > 100)
            INFO("Timer[%X] already expired by %u msec on thread '%s'.",
                 expiredTimer, -waitTime, name());
          uint64_t start = Sys::millis();
          expiredTimer->request();
          uint32_t deltaExec = Sys::millis() - start;
          if (deltaExec > 50)
            WARN("Timer [%X] request slow %d msec on thread '%s'", expiredTimer,
                 deltaExec, name());
        }
      }
    }
  }
  catch (const char *msg)
  {
    ERROR("EXCEPTION => %s ", msg);
    while (1)
    { // wait for watchdog to reset
    };
  }
}
#endif
