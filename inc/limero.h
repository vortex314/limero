#ifndef LIMERO_H
#define LIMERO_H
#include <Log.h>
#include <Sys.h>
#include <errno.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#define STRINGIFY(X) #X
#define S(X) STRINGIFY(X)
// ------------------------------------------------- Linux
#if defined(LINUX)
#include <thread>
#endif
//--------------------------------------------------  ESP8266
#if defined(ESP_OPEN_RTOS) || \
    defined(ESP8266_RTOS_SDK)  // ESP_PLATFORM for ESP8266_RTOS_SDK
#define FREERTOS
#define NO_ATOMIC
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#endif
//-------------------------------------------------- STM32
#ifdef STM32_OPENCM3
#define FREERTOS
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#endif
//-------------------------------------------------- ESP32
#if defined(ESP32_IDF)
#define FREERTOS
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "esp_system.h"
#include "freertos/task.h"
//#include "nvs.h"
//#include "nvs_flash.h"
#define PRO_CPU 0
#define APP_CPU 1
#endif
//-------------------------------------------------- ARDUINO
#ifdef ARDUINO
//#define NO_ATOMIC
#include <Arduino.h>
#include <printf.h>
#include <stdarg.h>
#undef INFO
#undef WARN
using cstr = const char *const;

#define INFO(fmt, ...)                                                      \
  {                                                                         \
    char line[256];                                                         \
    int len = snprintf(line, sizeof(line), "I %u | %.12s:%.3d | ",          \
                       (uint32_t)Sys::millis(), __SHORT_FILE__, __LINE__);  \
    snprintf((char *)(line + len), sizeof(line) - len, fmt, ##__VA_ARGS__); \
    Serial.println(line);                                                   \
    Serial.flush();                                                         \
  }
#define WARN(fmt, ...)                                                      \
  {                                                                         \
    char line[256];                                                         \
    int len = snprintf(line, sizeof(line), "W %u | %.12s:%.3d | ",          \
                       (uint32_t)Sys::millis(), __SHORT_FILE__, __LINE__);  \
    snprintf((char *)(line + len), sizeof(line) - len, fmt, ##__VA_ARGS__); \
    Serial.println(line);                                                   \
    Serial.flush();                                                         \
  }
#else
#include <Log.h>
#endif
//------------------------------------------------------------------------------------
#ifndef NO_ATOMIC
#include <atomic>
#endif
#undef min
#undef max

typedef struct {
  uint32_t bufferOverflow = 0;
  uint32_t bufferPushBusy = 0;
  uint32_t bufferPopBusy = 0;
  uint32_t threadQueueOverflow = 0;
  uint32_t bufferPushCasFailed = 0;
  uint32_t bufferPopCasFailed = 0;
  uint32_t bufferCasRetries = 0;
} NanoStats;
extern NanoStats stats;

//______________________________________________________________________
// INTERFACES nanoAkka
//

template <class T>
class AbstractQueue {
 public:
  virtual bool pop(T &t) = 0;
  virtual bool push(const T &t) = 0;  // const to be able to do something like
                                      // push({"topic","message"});
};
//--------------- give an object a name, useful for debugging
class Named {
  std::string _name = "no-name";

 public:
  Named(const char *name) { _name = name == 0 ? "NULL" : name; }
  const char *name() { return _name.c_str(); }
};
//--------------- something that can be invoked or execute something
class Invoker {
 public:
  virtual void invoke() = 0;
};
//-------------- handler class for certain messages or events
template <class T>
class Subscriber {
 public:
  virtual void on(const T &t) = 0;
  virtual ~Subscriber(){};
};
#include <bits/atomic_word.h>
//------------- handler function for certain messages or events
template <class T>
class SubscriberFunction : public Subscriber<T> {
  std::function<void(const T &t)> _func;

 public:
  SubscriberFunction(std::function<void(const T &t)> func) { _func = func; }
  void on(const T &t) { _func(t); }
};
//------------ generator of messages
template <class T>
class Publisher {
 public:
  virtual void subscribe(Subscriber<T> *listener) = 0;
  void operator>>(Subscriber<T> &listener) { subscribe(&listener); }
  void operator>>(Subscriber<T> *listener) { subscribe(listener); }
  void operator>>(std::function<void(const T &t)> func) {
    subscribe(new SubscriberFunction<T>(func));
  }
};
//-----------------  can be prooked to publish something
class Requestable {
 public:
  virtual void request() = 0;
};
//___________________________________________________________________________
// lockfree buffer, isr ready
//
//#define BUSY (1 << 15) // busy read or write ptr

#if defined(ESP_OPEN_RTOS) || defined(ESP8266_IDF)
// Set Interrupt Level
// level (0-15),
// level 15 will disable ALL interrupts,
// level 0 will enable ALL interrupts
//
#define xt_rsil(level)                                               \
  (__extension__({                                                   \
    uint32_t state;                                                  \
    __asm__ __volatile__("rsil %0," STRINGIFY(level) : "=a"(state)); \
    state;                                                           \
  }))
#define xt_wsr_ps(state) \
  __asm__ __volatile__("wsr %0,ps; isync" ::"a"(state) : "memory")
#define interrupts() xt_rsil(0)
#define noInterrupts() xt_rsil(15)
#endif
//#pragma GCC diagnostic ignored "-Warray-bounds"

#ifdef ARDUINO
template <class T>
class ArrayQueue : public AbstractQueue<T> {
  T *_array;
  int _size;
  int _readPtr;
  int _writePtr;
  inline int next(int idx) { return (idx + 1) % _size; }

 public:
  ArrayQueue(int size) : _size(size) {
    _readPtr = _writePtr = 0;
    _array = (T *)new T[size];
    ;
  }
  bool push(const T &t) {
    //   INFO("push %X", (unsigned int)this);
    noInterrupts();
    int expected = _writePtr;
    int desired = next(expected);
    if (desired == _readPtr) {
      stats.bufferOverflow++;
      interrupts();
      return false;
    }
    _writePtr = desired;
    _array[desired] = t;
    interrupts();
    return true;
  }

  bool pop(T &t) {
    noInterrupts();
    int expected = _readPtr;
    int desired = next(expected);
    if (expected == _writePtr) {
      interrupts();
      return false;
    }
    _readPtr = desired;
    t = _array[desired];
    interrupts();
    return true;
  }
  size_t capacity() const { return _size; }
  size_t size() const {
    if (_writePtr > _readPtr)
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

#if defined(STM32_OPENCM3)
#define INDEX_TYPE uint32_t
template <typename T, size_t cache_line_size = 32>
#elif defined(ESP32_IDF)
#include <malloc.h>
#define INDEX_TYPE uint32_t
template <typename T, size_t cache_line_size = 4>
#else
template <typename T, size_t cache_line_size = 64>
#define INDEX_TYPE uint64_t
#endif

class ArrayQueue {
 private:
  struct alignas(cache_line_size) Item {
    T value;
    std::atomic<INDEX_TYPE> version;
  };

  struct alignas(cache_line_size) AlignedAtomicU64
      : public std::atomic<INDEX_TYPE> {
    using std::atomic<INDEX_TYPE>::atomic;
  };

  Item *m_items;
  size_t m_capacity;

  // Make sure each index is on a different cache line
  AlignedAtomicU64 m_head;
  AlignedAtomicU64 m_tail;

 public:
  explicit ArrayQueue(size_t capacity)
#if defined(STM32_OPENCM3) || defined(ESP8266_RTOS_SDK)
      : m_items(static_cast<Item *>(malloc(sizeof(Item) * capacity))),
        m_capacity(capacity),
        m_head(0),
        m_tail(0)
#else
      // m_items(static_cast<Item*>(aligned_alloc(cache_line_size,sizeof(Item) *
      // capacity ))),
      //      m_capacity(capacity), m_head(0), m_tail(0)
      : m_capacity(capacity),
        m_head(0),
        m_tail(0)

#endif
  {
    m_items = new Item[capacity];
    for (size_t i = 0; i < capacity; ++i) {
      m_items[i].version = i;
    }
  }

  virtual ~ArrayQueue() { free(m_items); }

  // non-copyable
  ArrayQueue(const ArrayQueue<T> &) = delete;
  ArrayQueue(const ArrayQueue<T> &&) = delete;
  ArrayQueue<T> &operator=(const ArrayQueue<T> &) = delete;
  ArrayQueue<T> &operator=(const ArrayQueue<T> &&) = delete;

  bool push(const T &value) {
    INDEX_TYPE tail = m_tail.load(std::memory_order_relaxed);

    if (m_items[tail % m_capacity].version.load(std::memory_order_acquire) !=
        tail) {
      return false;
    }

    if (!m_tail.compare_exchange_strong(tail, tail + 1,
                                        std::memory_order_relaxed)) {
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

  bool pop(T &out) {
    INDEX_TYPE head = m_head.load(std::memory_order_relaxed);

    // Acquire here makes sure read of m_data[head].value is not reordered
    // before this Also makes sure side effects in try_enqueue are visible here
    if (m_items[head % m_capacity].version.load(std::memory_order_acquire) !=
        (head + 1)) {
      return false;
    }

    if (!m_head.compare_exchange_strong(head, head + 1,
                                        std::memory_order_relaxed)) {
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

// STREAMS
class TimerSource;
//____________________________________________________________________ THREAD __
struct ThreadProperties {
  const char *name;
  int stackSize;
  int queueSize;
  int priority;
};

class Thread : public Named {
#ifdef LINUX
  int _pipeFd[2];
  int _writePipe = 0;
  int _readPipe = 0;

#elif defined(FREERTOS)
  QueueHandle_t _workQueue = 0;

#elif defined(ARDUINO)
  ArrayQueue<Invoker *> _workQueue;
#endif
  uint32_t queueOverflow = 0;
  void createQueue();
  std::vector<TimerSource *> _timers;
  static int _id;
  int _queueSize;
  int _stackSize;
  int _priority;

 public:
  Thread(const char *name = "noname");
  Thread(ThreadProperties props);
  void start();
  int enqueue(Invoker *invoker);
  int enqueueFromIsr(Invoker *invoker);
  void run();
  void loop();
  void addTimer(TimerSource *ts) { _timers.push_back(ts); }
};

//_____________________________________________________________________ SOURCE _
//
template <class T>
class Source : public Publisher<T>, public Requestable {
  std::vector<Subscriber<T> *> _listeners;
  T _last;

 public:
  void subscribe(Subscriber<T> *listener) { _listeners.push_back(listener); }
  void emit(const T &t) {
    _last = t;
    for (Subscriber<T> *listener : _listeners) {
      listener->on(t);
    }
  }
  ~Source() { WARN(" Source destructor. Really ? "); }
  void last(T &last) { last = _last; }
};
//____________________________________________________________ LAMBDASOURCE
//
template <class T>
class LambdaSource : public Source<T> {
  std::function<T()> _handler;

 public:
  LambdaSource(std::function<T()> handler) : _handler(handler){};
  T operator()() { return _handler(); }
  void request() { this->emit(_handler()); }
};
//_____________________________________________________________ RefSource
//
template <class T>
class RefSource : public Source<T> {
  T &_t;
  //  bool _pass = true;

 public:
  RefSource(T &t) : _t(t){};
  void request() { this->emit(_t); }
  void operator=(T t) {
    _t = t;
    //   if (_pass)
    this->emit(_t);
  }
  //  void pass(bool p) { _pass = p; }
  T &operator()() { return _t; }
};
//__________________________________________________________________________
//
template <class T>
class ValueSource : public Source<T> {
  T _t;

 public:
  ValueSource(){};
  ValueSource(T t) { _t = t; }
  void request() { this->emit(_t); }
  void operator=(T t) {
    _t = t;
    this->emit(_t);
  }
  T &operator()() { return _t; }
};
//__________________________________________________________________________`
//
// TimerSource
// id : the timer id send with the timer expiration
// interval : time after which the timer expires
// repeat : repetitive timer
//
// run : sink bool to stop or run timer
//	start : restart timer from now+interval
//_______________________________________________________________ TimerSource
//
class TimerMsg {
 public:
  TimerSource *source;
};

class TimerSource : public Source<TimerMsg>, public Named {
  uint32_t _interval = UINT32_MAX;
  bool _repeat = false;
  uint64_t _expireTime = UINT64_MAX;

  void setNewExpireTime() {
    uint64_t now = Sys::millis();
    _expireTime += _interval;
    if (_expireTime < now && _repeat) _expireTime = now + _interval;
  }

 public:
  TimerSource(Thread &thr, uint32_t interval = UINT32_MAX, bool repeat = false,
              const char *name = "unknownTimer1")
      : Named(name) {
    _interval = interval;
    _repeat = repeat;
    if (repeat) start();
    thr.addTimer(this);
  }
  /*
    TimerSource(const char *name = "unknownTimer3") : Named(name) {
      _expireTime = Sys::now() + _interval;
    };*/
  ~TimerSource() { WARN(" timer destructor. Really ? "); }

  // void attach(Thread &thr) { thr.addTimer(this); }
  void reset() { start(); }
  void start() { _expireTime = Sys::millis() + _interval; }
  void start(uint32_t interval) {
    _interval = interval;
    start();
  }
  void stop() { _expireTime = UINT64_MAX; }
  void interval(uint32_t i) { _interval = i; }
  void request() {
    if (Sys::millis() >= _expireTime) {
      if (_repeat)
        setNewExpireTime();
      else
        _expireTime = Sys::millis() + UINT32_MAX;
      TimerMsg tm = {this};
      this->emit(tm);
    }
  }
  void repeat(bool r) { _repeat = r; };
  uint64_t expireTime() { return _expireTime; }
  inline uint32_t interval() { return _interval; }
};
//____________________________________  SINK ______________________
template <class T>
class Sink : public Subscriber<T>, public Invoker, public Named {
  ArrayQueue<T> _queue;
  std::function<void(const T &)> _func;
  Thread *_thread = 0;
  T _lastValue;

 public:
  Sink(int capacity, const char *name = "unknown")
      : Named(name), _queue(capacity) {
    _func = [&](const T &t) {
      (void)t;
      WARN(" no handler attached to this sink ");
    };
  }
  ~Sink() { WARN(" Sink destructor. Really ? "); }
  Sink(int capacity, std::function<void(const T &)> handler,
       const char *name = "unknown")
      : Named(name), _queue(capacity), _func(handler){};

  void on(const T &t) {
    if (_thread) {
      if (_queue.push(t))
        _thread->enqueue(this);
      else {
        WARN("push failed on '%s' [%d/%d] ", name(), _queue.size(),
             _queue.capacity());
        stats.bufferOverflow++;
      }
    } else {
      _func(t);
    }
  }

  //  virtual void request() { invoke(); }
  void invoke() {
    if (_queue.pop(_lastValue)) {
      _func(_lastValue);
    } else
      WARN(" no data in queue '%s'[%d]", name(), _queue.capacity());
  }

  void async(Thread &thread, std::function<void(const T &)> func) {
    _func = func;
    _thread = &thread;
  }
  void sync(std::function<void(const T &)> func) {
    _thread = 0;
    _func = func;
  }
  //  T operator()() { return _lastValue; }
};

//_________________________________________________ Flow ________________
//
template <class IN, class OUT>
class Flow : public Subscriber<IN>, public Source<OUT> {
 public:
  void operator==(Flow<OUT, IN> &flow) {
    this->subscribe(&flow);
    flow.subscribe(this);
  };
};
// -------------------------------------------------------- Cache
template <class T>
class Cache : public Flow<T, T>, public Subscriber<TimerMsg> {
  Thread &_thread;
  uint32_t _minimum, _maximum;
  bool _unsendValue = false;
  uint64_t _lastSend;
  T _t;
  TimerSource _timerSource;

 public:
  Cache(Thread &thread, uint32_t minimum, uint32_t maximum,
        bool request = false)
      : _thread(thread),
        _minimum(minimum),
        _maximum(maximum),
        _timerSource(thread) {
    _timerSource.interval(minimum);
    _timerSource.start();
    _timerSource.subscribe(this);
  }
  void on(const T &t) {
    _t = t;
    uint64_t now = Sys::millis();
    _unsendValue = true;
    if (_lastSend + _minimum < now) {
      this->emit(t);
      _unsendValue = false;
      _lastSend = now;
    }
  }
  void on(const TimerMsg &tm) {
    uint64_t now = Sys::millis();
    if (_unsendValue) {
      this->emit(_t);
      _unsendValue = false;
      _lastSend = now;
    }
    if (now > _lastSend + _maximum) {
      this->emit(_t);
      _unsendValue = false;
      _lastSend = now;
    }
    _timerSource.reset();
  }
  void request() { this->emit(_t); }

  static Cache<T> &nw(Thread &t, uint32_t min, uint32_t max) {
    auto cache = new Cache<T>(t, min, max);
    return *cache;
  }
};
//_____________________________________________________________________________
//
template <class T>
class QueueFlow : public Flow<T, T>, public Invoker, public Named {
  ArrayQueue<T> _queue;
  Thread *_thread = 0;

 public:
  QueueFlow(size_t capacity, const char *name = "QueueFlow")
      : Named(name), _queue(capacity){};
  void on(const T &t) {
    if (_thread) {
      if (_queue.push(t))
        _thread->enqueue(this);
      else
        WARN(" push failed");
    } else {
      this->emit(t);
    }
  }
  void request() { invoke(); }
  void invoke() {
    T value;
    if (_queue.pop(value)) {
      this->emit(value);
    } else {
      WARN(" no data ");
    }
  }

  void async(Thread &thread) { _thread = &thread; }
  void sync(std::function<void(const T &)> func) { _thread = 0; }
};

//________________________________________________________________
//

template <class IN, class OUT>
class LambdaFlow : public Flow<IN, OUT> {
  std::function<bool(OUT &, const IN &)> _func;

 public:
  LambdaFlow() {
    _func = [](OUT &out, const IN &in) {
      WARN("no handler for this flow");
      return false;
    };
  };
  LambdaFlow(std::function<bool(OUT &, const IN &)> func) : _func(func){};
  void lambda(std::function<bool(OUT &, const IN &)> func) { _func = func; }
  virtual void on(const IN &in) {
    OUT out;
    if (_func(out, in)) this->emit(out);
  }
  void request(){};
  static LambdaFlow<IN, OUT> &nw(std::function<bool(OUT &, const IN &)> func) {
    auto lf = new LambdaFlow(func);
    return *lf;
  }
};

//________________________________________________________________
//
template <class IN, class OUT>
Source<OUT> &operator>>(Publisher<OUT> &publisher, Flow<IN, OUT> &flow) {
  publisher.subscribe(&flow);
  return flow;
}

//________________________________________________________________
//
template <class T>
class ValueFlow : public Flow<T, T> {
  T _t;
  bool _pass = true;

 public:
  ValueFlow(){};
  ValueFlow(T t) { _t = t; }
  void request() { this->emit(_t); }
  void operator=(T t) {
    _t = t;
    if (_pass) this->emit(_t);
  }
  T &operator()() { return _t; }
  void on(const T &in) {
    _t = in;
    this->emit(_t);
  }
  void pass(bool p) { _pass = p; }
};
//______________________________________ Actor __________________________
//
class Actor {
  Thread &_thread;

 public:
  Actor(Thread &thread) : _thread(thread) {}
  Thread &thread() { return _thread; }
};

#endif  // NANOAKKA_H
