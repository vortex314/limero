#ifndef SRC_SPINE_H_
#define SRC_SPINE_H_

#include "limero.h"
#include <Stringify.h>
#include <Protocol.h>

#define MAX_SIZE 1024 // > 384 hello 3 response

class RedisSpineCbor : public Actor
{
  ProtocolEncoder _cborOut;
  ProtocolDecoder _cborIn;

  ZeroFlow<bool> messageArrived;
  ZeroFlow<bool> pubArrived;

  std::string _loopbackTopic;
  std::string _latencyTopic;
  std::string _subscribePattern;
  TimerSource _loopbackTimer;
  TimerSource _connectionWatchdog;
  enum
  {
    CONNECTING,
    READY
  } _state;

public:
  QueueFlow<Bytes> rxdFrame;
  QueueFlow<Bytes> txdFrame;
  ValueFlow<bool> connected;
  std::string dstPrefix;
  std::string srcPrefix;
  std::string node;

  RedisSpineCbor(Thread &thread);
  void init();
  void setNode(const char *);

  void sendCbor(ProtocolEncoder &);
  void subscribe(const char *pattern);

  template <typename T>
  void publish(const char *topic, T t)
  {
    _cborOut.start().write('[').write("pub").write(srcPrefix).write('{').write(topic).write(t).write('}').write(']').end();
    sendCbor(_cborOut);
  }

  template <typename T>
  Sink<T> &publisher(std::string topic)
  {
    std::string absTopic = srcPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    SinkFunction<T> *sf =
        new SinkFunction<T>([&, absTopic](const T &t)
                            { 
                              if ( _state==READY ) 
                              publish(absTopic.c_str(), t); });
    return *sf;
  }

  template <typename T>
  Source<T> &subscriber(std::string topic)
  {
    std::string absTopic;

    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    else
      absTopic = dstPrefix + topic;
    auto vf = new ValueFlow<T>();
    pubArrived >> [&, absTopic, vf](const bool &)
    {
      std::string rcvTopic, cmd;
      T value;
      if (_cborIn.rewind().readArrayStart().read(cmd).read(rcvTopic).ok() && absTopic == rcvTopic)
      {
        if (_cborIn.read(value).ok())
          vf->emit(value);
      }
    };
    return *vf;
  }
};

#endif /* SRC_SPINE_H_ */
