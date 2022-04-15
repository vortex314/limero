/*
 * Spine.h
 *
 *  Created on: Nov 21, 2021
 *      Author: lieven
 */

#ifndef SRC_SPINE_H_
#define SRC_SPINE_H_

#include "ArduinoJson.h"
#include "limero.h"

#define FRAME_MAX_SIZE 128

class RedisSpine : public Actor
{
  DynamicJsonDocument _jsonOut;
  DynamicJsonDocument _jsonNested;
  DynamicJsonDocument _jsonIn;

  ValueFlow<bool> jsonArrived;

  std::string _loopbackTopic;
  std::string _latencyTopic;
  std::string _subscribePattern;
  TimerSource _loopbackTimer;
  TimerSource _connectionWatchdog;
  enum
  {
    CONNECTING,
    SUBSCRIBING,
    READY
  } _state;

public:
  ValueFlow<Bytes> rxdFrame;
  ValueFlow<Bytes> txdFrame;
  ValueFlow<bool> connected;
  std::string dstPrefix;
  std::string srcPrefix;
  std::string node;

  RedisSpine(Thread &thread);
  void init();
  void setNode(const char *);

  void sendJson(DynamicJsonDocument &json);
  void hello_3();
  void psubscribe(const char *pattern);

  template <typename T>
  void publish(const char *topic, T t)
  {
    if ( _state != READY ) return;
    std::string s;
    _jsonOut.clear();
    _jsonNested.clear();
    //   JsonVariant var = _jsonNested.to<JsonVariant>();
    //  var.set(t)
    _jsonNested.set(t);
    serializeJson(_jsonNested, s);
    _jsonOut[0] = "publish";
    _jsonOut[1] = topic;
    _jsonOut[2] = s;
    sendJson(_jsonOut);
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
    std::string absTopic = dstPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;

    auto vf = new ValueFlow<T>();
    jsonArrived >> [&](const bool &)
    {
      if (_jsonIn[0] == "pmessage" || _jsonIn[2] == absTopic)
        vf->emit(_jsonIn[2].as<T>());
    };
    return *vf;
  }
};

#endif /* SRC_SPINE_H_ */
