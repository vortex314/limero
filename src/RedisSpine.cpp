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
  { if ( _jsonIn[0] =="hello" && _jsonIn[1]["proto"]==3 ) {connected = true; _connectTimer.reset();} };
    jsonArrived >> [this](bool)
  { if ( _jsonIn[0] =="psubscribe"  ) {subscribed = true; _connectTimer.reset();} };

  _connectTimer >> [this](const TimerMsg &)
  { connected = false; };

  connected >> [this](bool b)
  {
    if (b)
    {
      subscribeNode();
    }
  };
}

void RedisSpine::init()
{
  _loopbackTimer >> [&](const TimerMsg &tm)
  {
    if (!connected())
      hello_3();
    else
      publish(_loopbackTopic.c_str(), Sys::micros());
  };

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

void RedisSpine::subscribeNode()
{
  _jsonOut.clear();
  _jsonOut[0] = "psubscribe";
  _jsonOut[1] = srcPrefix+"/*";
  sendJson(_jsonOut);
}