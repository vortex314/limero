/*
 * Spine.cpp
 *
 *  Created on: Nov 21, 2021
 *      Author: lieven
 */

#include "RedisSpineCbor.h"
#include "StringUtility.h"

RedisSpineCbor::RedisSpineCbor(Thread &thr, const char *nodeName)
    : Actor(thr),
      _cborOut(256), // initial size of the buffer
      _cborIn(256),
      _loopbackTimer(thr, 950, true, "loopbackTimer"),
      _connectionWatchdog(thr, 6000, true, "connectTimer"),
      rxdCbor(5, "Redis:rxdCbor"),
      txdCbor(5, "Redis:txdCbor")
{
  setNode(nodeName);
  _state = CONNECTING;
  connected = false;
  rxdCbor.async(thread());
  txdCbor.async(thread());
  
  rxdCbor >> [&](const Bytes &bs)
  {
    _cborIn.reset();
    _cborIn.put_bytes(bs);
    std::string cmd;
    if (_cborIn.rewind().read('[').read("pub").ok())
    {
      pubArrived.on(true);
    }
  };

  _loopbackTimer >> [&](const TimerMsg &)
  {
    static int cnt = 0;
    cnt++;
    if (_state == CONNECTING)
    {
      if (cnt & 1)
      {
        CborEncoder cborOut(128);
        cborOut.start().write('[').write("pub").write(_loopbackTopic).write(Sys::micros()).write(']').end();
        sendCbor(cborOut);
      }
      else
      {
        _cborOut.start().write('[').write("sub").write(_subscribePattern).write(']').end();
        sendCbor(_cborOut);
      }
    }
    else
    {
      CborEncoder cborOut(128);
      cborOut.start().write('[').write("pub").write(_loopbackTopic).write(Sys::micros()).write(']').end();
      sendCbor(cborOut);
    }
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
    _state = READY;
    _cborOut.start().write('[').write("pub").write(_latencyTopic).write(Sys::micros() - in).write(']').end();
    sendCbor(_cborOut);
    _connectionWatchdog.reset();
  };
};

void RedisSpineCbor::init()
{
}

void RedisSpineCbor::setNode(const char *n)
{
  node = n;
  srcPrefix = "src/" + node + "/";
  dstPrefix = "dst/" + node + "/";
  _loopbackTopic = dstPrefix + "system/loopback";
  _latencyTopic = srcPrefix + "system/latency";
  _aliveTopic = srcPrefix + "system/alive";
  _subscribePattern = dstPrefix + "*";
  connected = false;
}

void RedisSpineCbor::sendCbor(CborEncoder &out)
{
  txdCbor.on(out);
}

void RedisSpineCbor::subscribe(const char *pattern)
{
  _cborOut.start().write('[').write("sub").write(pattern).write(']').end();
  sendCbor(_cborOut);
}
