#ifndef LIMERO_H
#define LIMERO_H
#include <Log.h>
#include <StringUtility.h>
#include <Sys.h>
#include <errno.h>
#include <stdint.h>

#include <forward_list>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

typedef std::vector<uint8_t> Bytes;
typedef std::string String;
class TimerSource;

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
#ifdef CPU_STM32
#define interrupts() \
  { asm("cpsie i"); }
#define noInterrupts() \
  { asm("cpsid i"); }
#endif
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
#define PRO_CPU 0
#define APP_CPU 1
#endif
//-------------------------------------------------- ARDUINO
#ifdef ARDUINO
#include <Arduino.h>
#define NO_RTOS
#endif
//------------------------------------------------------------------------------------
#ifndef NO_ATOMIC
#include <atomic>
#endif
#undef min
#undef max

//--------------- give an object a name, useful for debugging
class Named {
  std::string _name = "no-name";

 public:
  Named(){};
  Named(const char *newName) { _name = newName == 0 ? "NULL" : newName; }
  const char *name() { return _name.c_str(); }
  void name(const char *newName) { _name = newName; }
  void name(const std::string &newName) { _name = newName; }
};
//-------------------------- Stack of named objects to backtrace events
class LogStack {
  std::vector<Named *> _stack;

 public:
#ifdef LINUX
  void clear() { _stack.clear(); }
  inline void push(Named *n) { _stack.push_back(n); }
  void pop() { _stack.pop_back(); }
  std::string toString() {
    std::string s;
    for (auto i : _stack) {
      s += i->name();
      s += "' -> '";
    }
    return s;
  }
#else
  inline void clear() {}
  inline void push(Named *) {}  // avoid code overhead
  inline void pop() {}
#endif
};
extern LogStack logStack;

class Thread;
//====================================================================================
struct Subscription {
  void *source;
  void *sink;

  static std::unordered_map<void *, std::forward_list<Subscription *> *>
      _subscriptionsPerSource;

  Subscription(void *src, void *snk) : source(src), sink(snk) {}

  ~Subscription() { INFO("dtor Subscription"); }

  static void addSource(void *source, std::forward_list<Subscription *> *list) {
    _subscriptionsPerSource.emplace(source, list);
  }

  static void eraseSink(void *sink) {
    INFO(" erase Sink from subscriptions : %X ", sink);
    auto it = _subscriptionsPerSource.begin();
    while (it != _subscriptionsPerSource.end()) {
      auto sub_list = it->second;
      auto prev = sub_list->before_begin();
      for (auto sl_it = sub_list->begin(); sl_it != sub_list->end();) {
        if ((*sl_it)->sink == sink) {
          delete (*sl_it);
          sl_it = sub_list->erase_after(prev);
        } else {
          prev = sl_it;
          sl_it++;
        }
      }
      it++;
    }
  }

  static void eraseSource(void *source) {
    INFO("erase source from _subscriptionsPerSource");
    _subscriptionsPerSource.erase(source);
  }
};
//______________________________________________________________________
//

template <class T>
class AbstractQueue {
 public:
  virtual bool pop(T &t) = 0;
  virtual bool push(const T &t) = 0;  // const to be able to do something like
                                      // push({"topic","message"});
};

//--------------- something that can be invoked or execute something
class Invoker {
 public:
  virtual void invoke() = 0;
};
//-------------- handler class for certain messages or events
template <class T>
class Sink : public Named {
 public:
  virtual void on(const T &t) = 0;
  virtual ~Sink() { Subscription::eraseSink(this); }
  Sink(const Sink &) = delete;
  Sink() = default;
};
#include <bits/atomic_word.h>
//------------- handler function for certain messages or events
template <class T>
class SinkFunction : public Sink<T> {
  std::function<void(const T &t)> _func;

 public:
  SinkFunction(std::function<void(const T &t)> func) { _func = func; }
  void on(const T &t) { _func(t); }
};
//------------ generator of messages
template <class IN, class OUT>
class LambdaFlow;

template <class T>
class Source : public Named {
  std::forward_list<Subscription *> _subscriptions;

 public:
  void subscribe(Sink<T> *listener) {
    _subscriptions.push_front(new Subscription(this, listener));
  }

  void unsubscribe(Sink<T> *listener) { _subscriptions.remove(listener); }

  Source() { Subscription::addSource(this, &_subscriptions); }

  ~Source() { Subscription::eraseSource(this); }
  Source(const Source &) = delete;

  void emit(const T &t) {
    if (_subscriptions.empty()) {
      WARN("no subscribers ");
      return;
    }
    for (auto sub : _subscriptions) {
      logStack.push(this);
      ((Sink<T> *)sub->sink)->on(t);
      logStack.pop();
    }
  }
  //  void operator>>(Sink<T> listener) { subscribe(&listener); }
  void operator>>(Sink<T> &listener) { subscribe(&listener); }
  void operator>>(Sink<T> *listener) { subscribe(listener); }
  void operator>>(std::function<void(const T &t)> func) {
    subscribe(new SinkFunction<T>(func));
  }
  template <class OUT>  // OUT is the type of the output
  void operator>>(std::function<bool(OUT &out, const T &t)> func) {
    subscribe(new LambdaFlow<T, OUT>(func));
  }
};
//-----------------  can be pooked to publish something
class Requestable {
 public:
  virtual void request() = 0;
};
//___________________________________________________________________________
// lockfree buffer, isr ready
//
//#define BUSY (1 << 15) // busy read or write ptr

#if defined(ESP_OPEN_RTOS) || defined(ESP8266_RTOS_SDK)
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

#ifdef NO_ATOMIC
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
  }
  bool push(const T &t) {
    //   INFO("push %X", (unsigned int)this);
    noInterrupts();
    int expected = _writePtr;
    int desired = next(expected);
    if (desired == _readPtr) {
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
  explicit ArrayQueue(size_t reserve)
#if defined(OPENCM3) || defined(ESP8266_RTOS_SDK) || PIOFRAMWORK == libopenCM3
      : m_items(static_cast<Item *>(malloc(sizeof(Item) * reserve))),
        m_capacity(reserve),
        m_head(0),
        m_tail(0)
#else
      // m_items(static_cast<Item*>(aligned_alloc(cache_line_size,sizeof(Item) *
      // capacity ))),
      //      m_capacity(capacity), m_head(0), m_tail(0)
      : m_capacity(reserve),
        m_head(0),
        m_tail(0)

#endif
  {
    m_items = new Item[reserve];
    for (size_t i = 0; i < reserve; ++i) {
      m_items[i].version = i;
    }
  }

  virtual ~ArrayQueue() { delete[] m_items; }

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

//____________________________________________________________________ THREAD __
struct ThreadProperties {
  const char *name;
  int stackSize;
  int queueSize;
  int priority;
};

typedef void (*CallbackFunction)(void *);
typedef struct {
  CallbackFunction fn;
  void *arg;
} Callback;

class Thread : public Named {
#ifdef LINUX
  int _pipeFd[2];
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

 private:
#if defined(FREERTOS)
  QueueHandle_t _workQueue = 0;
#elif defined(NO_RTOS)
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
  void addTimer(TimerSource *);
  static void loopAll();
};

//____________________________________________________________ LAMBDASOURCE
//
template <class T>
class LambdaSource : public Source<T>, public Requestable {
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

class TimerSource : public Source<TimerMsg>, public Requestable {
  uint32_t _interval = UINT32_MAX;
  bool _repeat = false;
  uint64_t _expireTime = UINT64_MAX;

  void setNewExpireTime() {
    uint64_t now = Sys::millis();
    _expireTime += _interval;
    if (_expireTime < now && _repeat) _expireTime = now + _interval;
  }

 public:
  TimerSource(Thread &thr, uint32_t __interval = UINT32_MAX, bool __repeat = false,
              const char *label = "unknownTimer1") {
    Source::name(
        stringFormat("TimerSource %s : %d msec", label, __interval).c_str());
    _interval = __interval;
    _repeat = __repeat;
    if (_repeat) start();
    thr.addTimer(this);
  }

  const char *name() { return Source::name(); }

  ~TimerSource() { WARN(" timer destructor. Really ? "); }

  // void attach(Thread &thr) { thr.addTimer(this); }
  void reset() { start(); }
  void start() { _expireTime = Sys::millis() + _interval; }
  void start(uint32_t __interval) {
    _interval = __interval;
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
      uint64_t now = Sys::millis();
      logStack.clear();
      logStack.push(this);
      this->emit(tm);
      uint32_t diff = Sys::millis() - now;
      if (diff > 10) WARN(" timer %s took %d ms to emit ", this->name(), diff);
    }
  }
  void repeat(bool r) { _repeat = r; };
  uint64_t expireTime() { return _expireTime; }
  inline uint32_t interval() { return _interval; }
};
//____________________________________  SINK ______________________

//_________________________________________________ Flow ________________
//
template <class IN, class OUT>
class Flow : public Sink<IN>, public Source<OUT> {
 public:
  ~Flow() { WARN(" Flow destructor. Really ? "); };
  void operator==(Flow<OUT, IN> &flow) {
    this->subscribe(&flow);
    flow.subscribe(this);
  };
};
// -------------------------------------------------------- Cache
template <class T>
class Cache : public Flow<T, T>, public Sink<TimerMsg> {
  Thread &_thread;
  uint32_t _minimum, _maximum;
  bool _unsendValue = false;
  uint64_t _lastSend;
  T _t;
  TimerSource _timerSource;

 public:
  Cache(Thread &thr, uint32_t minimum, uint32_t maximum)
      : _thread(thr),
        _minimum(minimum),
        _maximum(maximum),
        _timerSource(thr) {
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
class QueueFlow : public Flow<T, T>, public Invoker {
  ArrayQueue<T> _queue;
  Thread *_thread = 0;

 public:
  QueueFlow(size_t capacity, const char *label = "QueueFlow")
      : _queue(capacity) {
    Source<T>::name(label);
    Sink<T>::name(label);
  };
  void on(const T &t) {
    if (_thread) {
      if (_queue.push(t)) {
        if (_thread->enqueue(this)) WARN("enqueue failed");
      } else
        WARN("QueueFlow '%s' push failed", name());
    } else {
      WARN("QueueFlow '%s' no thread found", name());
      // this->emit(t);
    }
  }
  const char *name() { return Source<T>::name(); }
  void onIsr(const T &t) {
    if (_thread) {
      if (_queue.push(t)) {
        if (_thread->enqueueFromIsr(this)) {
          //          WARN("enqueueFromIsr failed");
        }
      }
      //      else
      //        WARN("QueueFlow '%s' push failed", name());
    } else {
      WARN("QueueFlow '%s' no thread found", name());
      // this->emit(t);
    }
  }
  void request() { invoke(); }
  void invoke() {
    T value;
    while (_queue.pop(value)) {
      uint64_t now = Sys::millis();
      this->emit(value);
      uint32_t delta = Sys::millis() - now;
      if (delta > 10) WARN("QueueFlow '%s' invoke too slow %d", name(), delta);
    }
    /* else
       WARN(" no data in queue ");*/
  }

  void async(Thread &thread) { _thread = &thread; }
  void sync() { _thread = 0; }
};

//________________________________________________________________
//

template <class IN, class OUT>
class LambdaFlow : public Flow<IN, OUT> {
  std::function<bool(OUT &, const IN &)> _func;

 public:
  LambdaFlow() {
    _func = [](OUT &, const IN &) {
      WARN("no handler for this flow");
      return false;
    };
  };
  LambdaFlow(std::function<bool(OUT &, const IN &)> func) : _func(func){};
  ~LambdaFlow() { WARN(" LambdaFlow destructor. Really ? "); };
  void lambda(std::function<bool(OUT &, const IN &)> func) { _func = func; }
  virtual void on(const IN &in) {
    OUT out;
    if (_func(out, in)) {
      this->emit(out);
    }
  }
  const char *name() { return Source<OUT>::name(); }
  void name(std::string &nme) {
    Source<OUT>::name(nme);
    Sink<IN>::name(nme);
  }
  void name(const char *_name) {
    Source<OUT>::name(_name);
    Sink<IN>::name(_name);
  }
  void request(){};
};

template <class T>
class Filter : public Flow<T, T> {
  std::function<bool(const T &t)> _func;

 public:
  Filter() {
    _func = [](const T &t) {
      WARN("no handler for this filter");
      return false;
    };
  };
  Filter(std::function<bool(const T &)> func) : _func(func){};
  virtual void on(const T &in) {
    if (_func(in)) this->emit(in);
  }
  void request(){};
};

template <typename T>
Flow<T, T> &filter(std::function<bool(const T &t)> f) {
  return *new Filter<T>(f);
}

//________________________________________________________________
//
template <class IN, class OUT>
Source<OUT> &operator>>(Source<IN> &publisher, Flow<IN, OUT> &flow) {
  publisher.subscribe(&flow);
  return flow;
}

template <class IN, class OUT>
Source<OUT> &operator>>(Source<IN> &publisher, Flow<IN, OUT> *flow) {
  publisher.subscribe(flow);
  return *flow;
}

template <class IN, class OUT>
Sink<IN> &operator>>(Flow<IN, OUT> &flow, Sink<OUT> &sink) {
  flow.subscribe(&sink);
  return flow;
}

template <class IN, class OUT1, class OUT2>
Source<OUT2> &operator>>(Flow<IN, OUT1> &flow1, Flow<OUT1, OUT2> &flow2) {
  flow1.subscribe(&flow2);
  return flow2;
}
template <class IN, class OUT>
Source<OUT> operator>>(Source<IN> &publisher,
                       std::function<bool(OUT &, const IN &)> func) {
  auto flow = new LambdaFlow<IN, OUT>(func);
  publisher.subscribe(flow);
  return flow;
}

//________________________________________________________________
//

template <class IN, class OUT>
Source<OUT> *operator>>(Source<IN> *publisher, Flow<IN, OUT> &flow) {
  publisher.subscribe(flow);
  return &flow;
}

template <class IN, class OUT>
Sink<IN> *operator>>(Flow<IN, OUT> *flow, Sink<OUT> &sink) {
  flow->subscribe(&sink);
  return flow;
}

template <class IN, class OUT1, class OUT2>
Source<OUT2> *operator>>(Flow<IN, OUT1> *flow1, Flow<OUT1, OUT2> &flow2) {
  flow1->subscribe(&flow2);
  return flow2;
}
/*
template <class T>
void operator>>(Source<T> *source, std::function<void(const T &t)> func) {
  source->subscribe(new SinkFunction<T>(func));
}*/
/*
template <class IN, class OUT>
Sink<IN> &operator>>(Flow<IN, OUT> *flow ,Sink<OUT> &sink )
{
  flow->subscribe(sink);
  return *flow;
}*/
/*
template <class IN,class OUT>
Sink<IN>& operator>>(Flow<IN,OUT>& flow,std::function<void(const OUT &t)> func){
  flow.subscribe(new SinkFunction<OUT>(func));
  return flow;
}

template <class IN,class OUT>
Sink<IN>& operator>>(Flow<IN,OUT>* flow,std::function<void(const OUT &t)> func){
  flow->subscribe(new SinkFunction<OUT>(func));
  return *flow;
}
*/
template <class T>
class ZeroFlow : public Flow<T, T> {
 public:
  ZeroFlow(){};
  void operator=(T t) { this->emit(t); }
  void on(const T &t) { this->emit(t); }
};
//________________________________________________________________
//
template <class T>
class ValueFlow : public Flow<T, T>, public Invoker, public Requestable {
  T _t;
  Thread *_thread = 0;

 public:
  ValueFlow(){};
  ValueFlow(T t) { _t = t; }
  ValueFlow(const ValueFlow &other) = delete;
  const char *name() { return Source<T>::name(); }
  void name(const char *_name) {
    Source<T>::name(_name);
    Sink<T>::name(_name);
  }
  T &value() { return _t; }
  void request() { this->emit(_t); }
  void operator=(T t) { on(t); }
  T &operator()() { return _t; }
  void on(const T &t) {
    _t = t;
    if (_thread)
      _thread->enqueue(this);
    else
      this->emit(t);
  }
  void invoke() { this->emit(_t); };
  void async(Thread &thread) { _thread = &thread; }
  void sync() { _thread = 0; }
};
//______________________________________ Actor __________________________
//
class Actor {
  Thread &_thread;

 public:
  Actor(Thread &thr) : _thread(thr) {}
  Thread &thread() { return _thread; }
};

//____________________________________________________________________________________
//
class Poller : public Actor {
  TimerSource _pollInterval;
  std::vector<Requestable *> _requestables;
  uint32_t _idx = 0;

 public:
  ValueFlow<bool> connected;
  ValueFlow<uint32_t> interval;
  Poller(Thread &t) : Actor(t), _pollInterval(t, 500, true) {
    connected.on(false);
    interval.on(500);
    _pollInterval >> [&](const TimerMsg) {
      if (_requestables.size() && connected())
        _requestables[_idx++ % _requestables.size()]->request();
    };
    interval >> [&](const uint32_t iv) { _pollInterval.interval(iv); };
  };

  /*
    template <class T> Source<T> &poll(Source<T> &source) {
      RequestFlow<T> *rf = new RequestFlow<T>(source);
      source >> rf;
      _requestables.push_back(rf);
      return *rf;
    }*/

  template <class T>
  LambdaSource<T> &operator>>(LambdaSource<T> &source) {
    _requestables.push_back(&source);
    return source;
  }

  template <class T>
  Source<T> &operator>>(Source<T> &source) {
    _requestables.push_back(&source);
    return source;
  }

  template <class T>
  ValueFlow<T> &operator>>(ValueFlow<T> &source) {
    _requestables.push_back(&source);
    return source;
  }

  template <class T>
  RefSource<T> &operator>>(RefSource<T> &source) {
    _requestables.push_back(&source);
    return source;
  }

  Poller &operator()(Requestable &rq) {
    _requestables.push_back(&rq);
    return *this;
  }
};

#endif  // LIMERO_H
