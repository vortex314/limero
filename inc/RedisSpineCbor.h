#ifndef SRC_SPINE_H_
#define SRC_SPINE_H_

#include <Cbor.h>
#include <Stringify.h>

#include "limero.h"

class RedisSpineCbor : public Actor {
  CborEncoder _cborOut;
  CborDecoder _cborIn;


  std::string _loopbackTopic;
  std::string _latencyTopic;
  std::string _aliveTopic;
  std::string _subscribePattern;
  TimerSource& _loopbackTimer;
  TimerSource& _connectionWatchdog;
  enum { CONNECTING, READY } _state;
  void setNode(const char *);

 public:
  ValueFlow<Bytes> rxdBytes;
  QueueFlow<Bytes> rxdCbor;
  QueueFlow<Bytes> txdCbor;
  ValueFlow<bool> pubArrived;
  ValueFlow<bool> connected;
  std::string dstPrefix;
  std::string srcPrefix;
  std::string node;

  RedisSpineCbor(Thread &thread, const char *node);
  void init();

  void sendCbor(CborEncoder &);
  void subscribe(const char *pattern);

  template <typename T>
  void publish(const char *topic, T t) {
    _cborOut.start()
        .write('[')
        .write("pub")
        .write(topic)
        .write(t)
        .write(']')
        .end();
    sendCbor(_cborOut);
  }

  template <typename T>
  Sink<T> &publisher(std::string topic) {
    std::string absTopic = srcPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    SinkFunction<T> *sf = new SinkFunction<T>([&, absTopic](const T &t) {
      if (_state == READY) publish(absTopic.c_str(), t);
    });
    return *sf;
  }

  template <typename T>
  Source<T> &subscriber(std::string topic) {
    std::string absTopic;

    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    else
      absTopic = dstPrefix + topic;
    auto vf = new ValueFlow<T>(thread(), "vf");
    pubArrived >> [&, absTopic, vf](const bool &) {
      std::string rcvTopic, cmd;
      T value;
      if (_cborIn.rewind().readArrayStart().read(cmd).read(rcvTopic).ok() &&
          absTopic == rcvTopic) {
        if (_cborIn.read(value).ok()) vf->emit(value);
      }
    };
    return *vf;
  }
};

#endif /* SRC_SPINE_H_ */
