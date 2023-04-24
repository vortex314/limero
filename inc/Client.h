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
#include <Stringify.h>

#define FRAME_MAX_SIZE 1024 // > 384 hello 3 response

class Client : public Actor
{
  DynamicJsonDocument _jsonOut;
  DynamicJsonDocument _jsonIn;
  DynamicJsonDocument _jsonNestedIn;

  ValueFlow<bool> messageArrived;
  ValueFlow<bool> pubArrived;
  TimerSource& _loopbackTimer;
  TimerSource& _connectionWatchdog;
  std::string _loopbackTopic;
  std::string _latencyTopic;
  std::string _subscribePattern;

  enum
  {
    CONNECTING,
    SUBSCRIBING,
    READY
  } _state;

public:
  QueueFlow<String> rxd;
  QueueFlow<String> txd;
  ValueFlow<bool> connected;
  std::string dstPrefix;
  std::string srcPrefix;
  std::string node;

  Client(Thread &thread);
  void init();
  void setNode(const char *);

  void sendJson(DynamicJsonDocument &json);
  void subscribe(const char *pattern);

  template <typename T>
  void publish(const char *topic, T t)
  {
    _jsonOut.clear();
    _jsonOut[0] = "pub";
    _jsonOut[1] = topic;
    _jsonOut[2] = t;
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
                              publish(absTopic.c_str(), t); },absTopic.c_str());
    return *sf;
  }

  template <typename T>
  Source<T> &subscriber(std::string topic)
  {
    std::string absTopic = dstPrefix + topic;
    if (topic.rfind("src/", 0) == 0 || topic.rfind("dst/", 0) == 0)
      absTopic = topic;

    auto vf = new ValueFlow<T>(thread(), topic.c_str());
    pubArrived >> [&, absTopic, vf](const bool &)
    {
      //     INFO("%s:%s", absTopic.c_str(), _jsonIn[2].as<std::string>().c_str());
      if (_jsonIn[2].as<std::string>() == absTopic)
        vf->emit(_jsonNestedIn.as<T>());
    };
    return *vf;
  }
};

#endif /* SRC_SPINE_H_ */
