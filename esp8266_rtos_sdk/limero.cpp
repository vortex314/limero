#include "limero.h"

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

Thread::Thread(const char *name) : Named(name) {
  _queueSize = 20;
  _stackSize = 5000;
  _priority = 17;
}

void Thread::addTimer(TimerSource *ts) { _timers.push_back(ts); }

Thread::Thread(ThreadProperties props) : Named(props.name) {
  _queueSize = props.queueSize ? props.queueSize : 20;
  _stackSize = props.stackSize ? props.stackSize : 256;
  _priority = props.priority ? props.priority : configMAX_PRIORITIES - 1;
}

void Thread::createQueue() {
  _workQueue = xQueueCreate(_queueSize, sizeof(Invoker *));
  if (_workQueue == NULL) WARN("Queue creation failed ");
}

void Thread::start() {
  createQueue();
  xTaskCreate([](void *task) { ((Thread *)task)->run(); }, name(), _stackSize,
              this, _priority, NULL);
}

int Thread::enqueue(Invoker *invoker) {
  if (_workQueue)
    if (xQueueSend(_workQueue, &invoker, (TickType_t)0) != pdTRUE) {
      WARN("Thread '%s' queue overflow [%X]", name(), invoker);
      return ENOBUFS;
    }
  return 0;
};
int Thread::enqueueFromIsr(Invoker *invoker) {
  if (_workQueue) {
    if (xQueueSendFromISR(_workQueue, &invoker, (TickType_t)0) != pdTRUE) {
      //  WARN("queue overflow"); // cannot log here concurency issue
      return ENOBUFS;
    }
  }
  return 0;
};

void Thread::run() {
  INFO("Thread '%s' prio : %d started ", name(), uxTaskPriorityGet(NULL));
  createQueue();
  uint32_t noWaits = 0;
  while (true) {
    uint64_t now = Sys::millis();
    uint64_t expTime = now + 5000;
    TimerSource *expiredTimer = 0;
    // find next expired timer if any within 5 sec
    for (auto timer : _timers) {
      if (timer->expireTime() < expTime) {
        expTime = timer->expireTime();
        expiredTimer = timer;
      }
    }
    int32_t waitTime =
        (expTime - now);  // ESP_OPEN_RTOS seems to double sleep time ?

    //		INFO(" waitTime : %d ",waitTime);
    if (noWaits % 1000 == 999)
      WARN(" noWaits : %d in thread %s waitTime %d ", noWaits, name(),
           waitTime);
    if (waitTime > 0) {
      Invoker *prq;
      TickType_t tickWaits = pdMS_TO_TICKS(waitTime);
      if (tickWaits == 0) noWaits++;
      if (xQueueReceive(_workQueue, &prq, tickWaits) == pdPASS) {
        uint64_t start = Sys::millis();
        prq->invoke();
        uint32_t delta = Sys::millis() - start;
        if (delta > 50)
          WARN("Invoker [%X] slow %d msec invoker on thread '%s'.", prq, delta,
               name());
      } else {
        noWaits = 0;
      }
    } else {
      noWaits++;
      if (expiredTimer) {
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
