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
  DynamicJsonDocument _jsonIn;

  ValueFlow<bool> jsonArrived;

  std::string _loopbackTopic;
  std::string _latencyTopic;
  std::string _subscribePattern;
  TimerSource _loopbackTimer;
  TimerSource _connectionWatchdog;
  typedef enum {
    CONNECTING,
    SUBSCRIBING,
    READY
  } _state=CONNECTING;

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
  void psubscribe(const char* pattern);

  template <typename T>
  void publish(const char *topic, T v)
  {
    _jsonOut.clear();
    _jsonOut[0] = "publish";
    _jsonOut[1] = topic;
    _jsonOut[2] = v;
    sendJson(_jsonOut);
  }

  template <typename T>
  Sink<T> &publisher(std::string topic)
  {
    std::string absTopic = srcPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;
    SinkFunction<T> *sf = new SinkFunction<T>([&, absTopic](const T &t)
                                              {     
                                                _jsonOut.clear();
    _jsonOut[0] = "publish";
    _jsonOut[1] = absTopic;
    _jsonOut[2] = t;
    sendJson(_jsonOut); });
    return *sf;
  }

  template <typename T>
  Source<T> &subscriber(std::string topic)
  {
    std::string absTopic = dstPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;

    auto vf = new ValueFlow<T>();
    jsonArrived >> new SinkFunction<bool>([&](const bool &)
                                          {
                                            if (_jsonIn[0] == "pmessage" || _jsonIn[1] == absTopic)
          vf->emit( _jsonIn[2].as<T>()); });
    return *vf;
  }
};

#endif /* SRC_SPINE_H_ */
