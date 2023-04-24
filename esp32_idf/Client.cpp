#include "Client.h"
#include "StringUtility.h"

Client::Client(Thread &thr)
    : Actor(thr),
      _jsonOut(FRAME_MAX_SIZE),
      _jsonIn(FRAME_MAX_SIZE),
      _jsonNestedIn(FRAME_MAX_SIZE),
      messageArrived(thr, "Client:messageArrived"),
      pubArrived(thr, "Client:pubArrived"),
      _loopbackTimer(thr.createTimer(1000, true, true, "Client:loopbackTimer")),
      _connectionWatchdog(thr.createTimer(6000, true, true, "Client:connectTimer")),
      rxd(thr, 5, "Client:rxdFrame"),
      txd(thr, 5, "Client:txdFrame"),
      connected(thr, "Client:connected")

{
  setNode(Sys::hostname());
  _state = SUBSCRIBING;
  connected = false;
  rxd >> [&](const String &b)
  {
    _jsonIn.clear();
    auto error = deserializeJson(_jsonIn, b);
    if (error == DeserializationError::Ok && _jsonIn.is<JsonArray>())
    {
      messageArrived.on(true);
    }
    else
    {
      WARN("Client: deserializeJson error %d on %s ", error, b.c_str());
    }
  };

  messageArrived >> [&](bool)
  {
    if (_jsonIn[0] == "pub")
    {
      _state = READY;
      _jsonNestedIn.clear();
      if (DeserializationError::Ok == deserializeJson(_jsonNestedIn, _jsonIn[3].as<std::string>()))
        pubArrived.on(true);
    }
  };

  _loopbackTimer >> [&](const TimerSource &tm)
  {
    if (_state == SUBSCRIBING)
      subscribe(_subscribePattern.c_str());
    publish(_loopbackTopic.c_str(), Sys::micros());
  };

  _connectionWatchdog >> [this](const TimerSource &)
  {
    INFO("WDT connection")
    connected = false;
    _state = SUBSCRIBING;
  };

  subscriber<uint64_t>(_loopbackTopic) >> [&](const uint64_t &in)
  {
    connected = true;
    publish(_latencyTopic.c_str(), (Sys::micros() - in));
    _connectionWatchdog.start();
  };
};

void Client::init()
{
}

void Client::setNode(const char *n)
{
  node = n;
  srcPrefix = "src/" + node + "/";
  dstPrefix = "dst/" + node + "/";
  _loopbackTopic = dstPrefix + "system/loopback";
  _latencyTopic = srcPrefix + "system/latency";
  _subscribePattern = dstPrefix + "*";
  connected = false;
}

void Client::sendJson(DynamicJsonDocument &json)
{
  String jsonStr;
  serializeJson(json, jsonStr);
  txd.on(jsonStr);
}

void Client::subscribe(const char *pattern)
{
  _jsonOut.clear();
  _jsonOut[0] = "sub";
  _jsonOut[1] = pattern;
  sendJson(_jsonOut);
}
