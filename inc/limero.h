#ifndef __LIMERO_H__
#define __LIMERO_H__
#include <Log.h>
#include <Sys.h>
#include <stdio.h>

#include <forward_list>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <vector>

typedef std::string String;
typedef std::vector<uint8_t> Bytes;
#ifdef ESP32_IDF
#define FREERTOS
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "esp_system.h"
#include "freertos/task.h"
#define PRO_CPU 0
#define APP_CPU 1
#endif
//___________________________________________________________________________
// lockfree buffer, isr ready
//
// #define BUSY (1 << 15) // busy read or write ptr

#if defined(ESP_OPEN_RTOS) || defined(ESP8266_RTOS_SDK)
// Set Interrupt Level
// level (0-15),
// level 15 will disable ALL interrupts,
// level 0 will enable ALL interrupts
//
#define xt_rsil(level)                               \
  (__extension__({                                   \
    uint32_t state;                                  \
    __asm__ __volatile__("rsil %0," STRINGIFY(level) \
                         : "=a"(state));             \
    state;                                           \
  }))
#define xt_wsr_ps(state)                               \
  __asm__ __volatile__("wsr %0,ps; isync" ::"a"(state) \
                       : "memory")
#define interrupts() xt_rsil(0)
#define noInterrupts() xt_rsil(15)
#else

#endif
// #pragma GCC diagnostic ignored "-Warray-bounds"

#ifdef NO_ATOMIC
template <class T>
class ArrayQueue : public AbstractQueue<T>
{
  T *_array;
  int _size;
  int _readPtr;
  int _writePtr;
  inline int next(int idx) { return (idx + 1) % _size; }

public:
  ArrayQueue(int size) : _size(size)
  {
    _readPtr = _writePtr = 0;
    _array = (T *)new T[size];
  }
  bool push(const T &t)
  {
    //   INFO("push %X", (unsigned int)this);
    noInterrupts();
    int expected = _writePtr;
    int desired = next(expected);
    if (desired == _readPtr)
    {
      interrupts();
      //      WARN("ArrayQueue sz %d rd %d wr %d ", size(), _readPtr,
      //      _writePtr);
      return false;
    }
    _writePtr = desired;
    _array[desired] = t;
    interrupts();
    return true;
  }

  bool pop(T &t)
  {
    noInterrupts();
    int expected = _readPtr;
    int desired = next(expected);
    if (expected == _writePtr)
    {
      interrupts();
      return false;
    }
    _readPtr = desired;
    t = _array[desired];
    interrupts();
    return true;
  }
  size_t capacity() const { return _size; }
  size_t size() const
  {
    if (_writePtr >= _readPtr)
      return _writePtr - _readPtr;
    else
      return _writePtr + _size - _readPtr;
  }
};
#else
#pragma once

#include <stdlib.h>

#include <atomic>
#include <cstdint>

#if defined(STM32)
#define INDEX_TYPE uint32_t
template <typename T, size_t cache_line_size = 4>
#elif defined(ESP32_IDF)
#include <malloc.h>
#define INDEX_TYPE uint32_t
template <typename T, size_t cache_line_size = 4>
#else
template <typename T, size_t cache_line_size = 8>
#define INDEX_TYPE uint64_t
#endif

class ArrayQueue
{
private:
  struct alignas(cache_line_size) Item
  {
    T value;
    std::atomic<INDEX_TYPE> version;
  };

  struct alignas(cache_line_size) AlignedAtomicU64
      : public std::atomic<INDEX_TYPE>
  {
    using std::atomic<INDEX_TYPE>::atomic;
  };

  Item *m_items;
  size_t m_capacity;

  // Make sure each index is on a different cache line
  AlignedAtomicU64 m_head;
  AlignedAtomicU64 m_tail;

public:
  explicit ArrayQueue(size_t reserve)
#if defined(OPENCM3) || defined(ESP8266_RTOS_SDK) || PIOFRAMWORK == libopenCM3
      : m_items(static_cast<Item *>(malloc(sizeof(Item) * reserve))),
        m_capacity(reserve),
        m_head(0),
        m_tail(0)
#else
      : m_capacity(reserve),
        m_head(0),
        m_tail(0)
#endif
  {
    m_items = new Item[reserve];
    for (size_t i = 0; i < reserve; ++i)
    {
      m_items[i].version = i;
    }
  }

  virtual ~ArrayQueue() { delete[] m_items; }

  // non-copyable
  ArrayQueue(const ArrayQueue<T> &) = delete;
  ArrayQueue(const ArrayQueue<T> &&) = delete;
  ArrayQueue<T> &operator=(const ArrayQueue<T> &) = delete;
  ArrayQueue<T> &operator=(const ArrayQueue<T> &&) = delete;

  bool push(const T &value)
  {
    INDEX_TYPE tail = m_tail.load(std::memory_order_relaxed);

    if (m_items[tail % m_capacity].version.load(std::memory_order_acquire) !=
        tail)
    {
      return false;
    }

    if (!m_tail.compare_exchange_strong(tail, tail + 1,
                                        std::memory_order_relaxed))
    {
      return false;
    }

    m_items[tail % m_capacity].value = value;

    // Release operation, all reads/writes before this store cannot be reordered
    // past it Writing version to tail + 1 signals reader threads when to read
    // payload
    m_items[tail % m_capacity].version.store(tail + 1,
                                             std::memory_order_release);
    return true;
  }

  bool pop(T &out)
  {
    INDEX_TYPE head = m_head.load(std::memory_order_relaxed);

    // Acquire here makes sure read of m_data[head].value is not reordered
    // before this Also makes sure side effects in try_enqueue are visible here
    if (m_items[head % m_capacity].version.load(std::memory_order_acquire) !=
        (head + 1))
    {
      return false;
    }

    if (!m_head.compare_exchange_strong(head, head + 1,
                                        std::memory_order_relaxed))
    {
      return false;
    }
    out = m_items[head % m_capacity].value;

    // This signals to writer threads that they can now write something to this
    // index
    m_items[head % m_capacity].version.store(head + m_capacity,
                                             std::memory_order_release);
    return true;
  }

  size_t capacity() const { return m_capacity; }
  size_t size() const { return m_tail - m_head; }
};

#endif

class Invoker
{
public:
  virtual void invoke() = 0;
};
typedef void (*CallbackFunction)(void *);
typedef struct
{
  CallbackFunction fn;
  void *arg;
} Callback;

//============================================================================
class Named
{
  std::string _name;

public:
  Named(const char *nm = "no-name") { _name = nm; };
  const char *name() { return _name.c_str(); };
  void name(const char *pref)
  {
    char buffer[60];
#ifdef LINUX
    snprintf(buffer, sizeof(buffer), "%s[%lX]", pref, (uint64_t)this);
#else
    snprintf(buffer, sizeof(buffer), "%s[%llX]", pref, (uint64_t)this);
#endif
    _name = buffer;
  };
};
//============================================================================

template <typename T>
class Source;

template <typename T>
class Sink : public Named
{
  std::function<void(const T &)> _handler;
  std::vector<Source<T> *> _sources;
  Sink(const Sink &) = delete; // forbid copy

public:
  Sink(const char *nm = "Sink")
  {
    name(nm);
    _handler = [](const T &t)
    { INFO("Default Sink handler called ?? "); };
  }
  Sink(std::function<void(const T &)> f)
  {
    name("SinkFunction");
    _handler = f;
  }

  ~Sink()
  {
    INFO("~Sink()");
    for (auto &source : _sources)
    {
      source->unsubscribe(this);
    }
  }
  void handler(std::function<void(const T &)> f) { _handler = f; }

  void addSource(Source<T> *source) { _sources.push_back(source); }

  virtual void on(const T &value) { _handler(value); }
};
//============================================================================

template <typename T>
class Source : public Named
{
  std::vector<Sink<T> *> _sinks;
  Source(const Source &) = delete; // forbid copy

public:
  Source(const char *nm = "Source") { name(nm); }
  ~Source() { INFO("~Source()"); }
  void emit(const T &value)
  {
    for (auto &sink : _sinks)
    {
      sink->on(value);
    }
  };
  void subscribe(Sink<T> *sink)
  {
    INFO("%s >> %s", name(), sink->name());
    sink->addSource(this);
    _sinks.push_back(sink);
  }
  void unsubscribe(Sink<T> *sink)
  {
    INFO("%s >> %s Delete", this->name(), sink->name());
    std::remove(_sinks.begin(), _sinks.end(), sink);
  }

  void operator>>(Sink<T> &sink) { subscribe(&sink); }
  void operator>>(std::function<void(const T &)> f)
  {
    Sink<T> *sink = new Sink<T>(f);
    subscribe(sink);
  }
};
//============================================================================
template <typename IN, typename OUT>
class Flow : public Sink<IN>, public Source<OUT>
{
  std::function<bool(OUT &, const IN &)> _func;

public:
  Flow(
      std::function<bool(OUT &, const IN &)> func =
          [](OUT &, const IN &)
      {
        INFO("no handler here");
        return false;
      },
      const char *__name = "Flow")
      : _func(func)
  {
    name(__name);
  }
  void name(const char *__name)
  {
    Sink<IN>::name(__name);
    Source<OUT>::name(__name);
  }
  void on(const IN &in)
  {
    OUT _out;
    if (_func(_out, in))
      this->emit(_out);
  }
};
template <typename IN, typename OUT>
Source<OUT> &operator>>(Source<IN> &source, Flow<IN, OUT> &flow)
{
  source.subscribe(&flow);
  return flow;
}
//============================================================================

//============================================================================

class TimerSource : public Source<TimerSource>, public Invoker
{
  uint64_t _expireTime = UINT64_MAX;
  uint32_t _interval = 5000;
  bool _repeat = false;
  bool _active = true;

public:
  TimerSource(uint32_t intv, bool repeat, bool active = true,
              const char *__name = "Timer")
      : Source<TimerSource>(__name)
  {
    _interval = intv;
    _repeat = repeat, _active = active;
    _expireTime = Sys::millis() + _interval;
  }
  void interval(uint32_t interv) { _interval = interv; };
  void repeat(bool b) { _repeat = true; };

  uint64_t expiresAt()
  {
    if (_active)
      return _expireTime;
    else
      return UINT64_MAX;
  }
  bool isExpired() { return (_active && _expireTime < Sys::millis()); }
  void start()
  {
    _active = true;
    _expireTime = Sys::millis() + _interval;
  }
  void stop() { _active = false; }
  void invoke()
  {
    emit(*this);
    if (_active)
      _expireTime = Sys::millis() + _interval;
    else
      _expireTime = UINT64_MAX;
  };
};
/*#define THREAD_LOCK(__thread__)                           \
  const std::lock_guard<std::recursive_mutex> actor_lock( \
      __thread__->thread_mutex)
#define THREAD_LOCK_TRY(__thread__) __thread__->thread_mutex.try_lock()
*/
#ifdef FREERTOS
struct ThreadProperties
{
  const char *name;
  int stackSize;
  int queueSize;
  int priority;
};
#endif

class Thread : public Named
{
  //  ArrayQueue<Invoker *> _invokers;
  std::vector<TimerSource *> _timers;
#ifdef LINUX
  int _pipeFd[2]; // pipe for thread wakeup and invoker queue
  int _writePipe = 0;
  int _readPipe = 0;
  fd_set _rfds;
  fd_set _wfds;
  fd_set _efds;
  int _maxFd;
  std::unordered_map<int, Callback> _writeInvokers;
  std::unordered_map<int, Callback> _readInvokers;
  std::unordered_map<int, Callback> _errorInvokers;
  void buildFdSet();

public:
  void addReadInvoker(int, void *, CallbackFunction);
  void addWriteInvoker(int, void *, CallbackFunction);
  void addErrorInvoker(int, void *, CallbackFunction);
  void delReadInvoker(const int);
  void delWriteInvoker(const int);
  void delErrorInvoker(const int);
  void delAllInvoker(int);
  int waitInvoker(uint32_t timeout);
#endif
#if defined(FREERTOS)
private:
  QueueHandle_t _workQueue = 0; // queue for thread wakeup and invoker queue
  int _queueSize = 10;
  int _stackSize = 1024;
  int _priority = tskIDLE_PRIORITY + 1;

public:
  Thread(ThreadProperties props);

  //    void createQueue(int queueSize = 10, int stackSize = 1024, int priority
  //    = 1);
  int queueFromISR(Invoker *invoker);

#endif

public:
  Thread(const char *_name = "Thread");
  // bool try_lock() { return thread_mutex.try_lock(); }
  void run();
  void stop();
  void start();
  void wake();
  void wait(uint32_t timeout);
  int queue(Invoker *invoker);
  void createQueue();
  uint32_t minWait();
  TimerSource &createTimer(uint32_t interval, bool repeat, bool active = true,
                           const char *__name = "Timer");
  void deleteTimer(TimerSource &timer);
};
//============================================================================

class Actor : public Named
{
  Thread &_thread;

public:
  Actor(Thread &thr) : _thread(thr) { name("Actor"); }

  virtual void run() { INFO(" Default run of actor !"); }

  Thread &thread() { return _thread; }
};
//============================================================================

//============================================================================
template <typename T>
class QueueFlow : public Flow<T, T>, public Invoker
{
  Thread &_thread;
  ArrayQueue<T> _queue;
  T _t;

public:
  QueueFlow<T>(Thread thread, size_t size = 1,const char *__name = "QueueFlow"): _thread(thread), _queue(size)
  {
    ((Flow<T, T> *)this)->name(__name);
  }
  void invoke() // from owning thread
  {
    while (_queue.pop(_t))
    {
      this->emit(_t);
    }
  }
  void on(const T &t) // from other thread or same thread
  {
    _queue.push(t);
    _thread.queue(this);
  }
  T &last() { return _t; }
};
template <typename T>
class ValueFlow : public QueueFlow<T>
{
public:
  ValueFlow<T>(Thread &thread, const char *__name = "ValueFlow")
      : QueueFlow<T>(thread, 1, __name)
  {
    ((Flow<T, T> *)this)->name(__name);
  }
  const T &operator()() { return this->last(); }
  void operator=(const T &t) { this->on(t); }
};

#endif
