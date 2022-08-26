/*
 * Spine.cpp
 *
 *  Created on: Nov 21, 2021
 *      Author: lieven
 */

#include "RedisSpine.h"
#include "StringUtility.h"

RedisSpine::RedisSpine(Thread &thr)
    : Actor(thr),
      _jsonOut(FRAME_MAX_SIZE),
      _jsonIn(FRAME_MAX_SIZE),
      _jsonNestedIn(FRAME_MAX_SIZE),
      _loopbackTimer(thr, 1000, true, "loopbackTimer"),
      _connectionWatchdog(thr, 6000, true, "connectTimer"),
      rxdFrame(5, "Redis:rxdFrame"),
      txdFrame(5, "Redis:txdFrame")
{
  setNode(Sys::hostname());
  _state = CONNECTING;
  connected = false;
  rxdFrame.async(thread());
  txdFrame.async(thread());
  rxdFrame >> [&](const Bytes &b)
  {
    _jsonIn.clear();
    auto error = deserializeJson(_jsonIn, b.data(), b.size());
    if (error == DeserializationError::Ok && _jsonIn.is<JsonArray>())
    {
      messageArrived.on(true);
    }
    else
    {
      WARN("RedisSpine: deserializeJson error %d on %s ", error,charDump(b).c_str());
    }
  };

  messageArrived >> [&](bool)
  {
    if (_jsonIn[0] == "hello" && _jsonIn[1]["proto"] == 3)
    {
      _state = SUBSCRIBING;
      psubscribe(_subscribePattern.c_str());
    }
    else if (_jsonIn[0] == "psubscribe" && _jsonIn[1] == _subscribePattern)
    {
      _state = READY;
      publish(_loopbackTopic.c_str(), Sys::micros());
    }
    else if (_jsonIn[0] == "pmessage")
    {
      _jsonNestedIn.clear();
        if ( DeserializationError::Ok == deserializeJson(_jsonNestedIn, _jsonIn[3].as<std::string>()))
          pubArrived.on(true);
    }
  };

  _loopbackTimer >> [&](const TimerMsg &tm)
  {
    if (_state == CONNECTING)
      hello_3();
    if (_state == SUBSCRIBING)
      psubscribe(_subscribePattern.c_str());
    if (_state == READY)
      publish(_loopbackTopic.c_str(), Sys::micros());
  };

  _connectionWatchdog >> [this](const TimerMsg &)
  {
    INFO("WDT connection")
    connected = false;
    _state = CONNECTING;
  };

  subscriber<uint64_t>(_loopbackTopic) >> [&](const uint64_t &in)
  {
    connected = true;
    publish(_latencyTopic.c_str(), (Sys::micros() - in));
    _connectionWatchdog.reset();
  };
};

void RedisSpine::init()
{
}

void RedisSpine::setNode(const char *n)
{
  node = n;
  srcPrefix = "src/" + node + "/";
  dstPrefix = "dst/" + node + "/";
  _loopbackTopic = dstPrefix + "system/loopback";
  _latencyTopic = srcPrefix + "system/latency";
  _subscribePattern = dstPrefix + "*";
  connected = false;
}

void RedisSpine::sendJson(DynamicJsonDocument &json)
{
  std::string jsonStr;
  serializeJson(json, jsonStr);
  txdFrame.on(Bytes(jsonStr.begin(), jsonStr.end()));
}

void RedisSpine::hello_3()
{
  _jsonOut.clear();
  _jsonOut[0] = "hello";
  _jsonOut[1] = "3";
  sendJson(_jsonOut);
}

void RedisSpine::psubscribe(const char *pattern)
{
  _jsonOut.clear();
  _jsonOut[0] = "psubscribe";
  _jsonOut[1] = pattern;
  sendJson(_jsonOut);
}
