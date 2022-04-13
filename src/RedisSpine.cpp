/*
 * Spine.cpp
 *
 *  Created on: Nov 21, 2021
 *      Author: lieven
 */

#include "RedisSpine.h"
#include "StringUtility.h"

RedisSpine::RedisSpine(Thread &thread)
    : Actor(thread), _jsonOut(FRAME_MAX_SIZE), _jsonIn(FRAME_MAX_SIZE),
      _loopbackTimer(thread, 1000, true, "loopbackTimer"),
      _connectTimer(thread, 3000, true, "connectTimer")
{
  setNode(Sys::hostname());
  rxdFrame >> [&](const Bytes &b)
  {
    _jsonIn.clear();
    auto error = deserializeJson(_jsonIn, b.data(),b.size());
    if (error== DeserializationError::Ok  &&   _jsonIn.is<JsonArray>())
      jsonArrived.emit(true); };

  jsonArrived >> [this](bool)
  { if ( _jsonIn[0] =="hello" ) _connected = true; };
}

void RedisSpine::init()
{
  static uint32_t counter = 0;
  _loopbackTimer >> [&](const TimerMsg &tm)
  {
    if (!connected())
    {
      if (counter++ % 2 == 1)
        sendNode(node);
      else
        publish(_loopbackTopic.c_str(), Sys::micros());
    }
    else
      publish(_loopbackTopic.c_str(), Sys::micros());
  };

  _connectTimer >> [&](const TimerMsg &)
  { connected = false; };

  subscriber<uint64_t>(_loopbackTopic) >> [&](const uint64_t &in)
  {
    publish(_latencyTopic.c_str(), (Sys::micros() - in));
    connected = true;
    _connectTimer.reset();
  };
}

void RedisSpine::setNode(const char *n)
{
  node = n;
  srcPrefix = "src/" + node + "/";
  dstPrefix = "dst/" + node + "/";
  _loopbackTopic = dstPrefix + "system/loopback";
  _latencyTopic = srcPrefix + "system/latency";
  connected = false;
}

//================================= BROKER COMMANDS
//=========================================
void RedisSpine::sendNode(std::string &topic)
{

  txdFrame.on(Bytes(topic.begin(), topic.end()));
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
  _jsonOut[1] = 3;
  sendJson(_jsonOut);
}
