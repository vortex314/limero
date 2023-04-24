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
      _loopbackTimer(thr.createTimer( 950, true,true, "loopbackTimer")),
      _connectionWatchdog(thr.createTimer( 6000, true,true, "connectTimer")),
      rxdBytes(thr,"Redis:rxdBytes"),
      rxdCbor(thr,5, "Redis:rxdCbor"),
      txdCbor(thr,5, "Redis:txdCbor"),
      pubArrived(thr, "Redis:pubArrived"),
      connected(thr, "Redis:connected")
{
  setNode(nodeName);
  _state = CONNECTING;
  connected = false;

  rxdCbor >> [&](const Bytes &bs)
  {
    _cborIn.reset();
    _cborIn.put_bytes(bs);
    if (_cborIn.rewind().read('[').read("pub").ok())
    {
      pubArrived.on(true);
    } else {
      INFO(" unknown message");
    }
  };

  _loopbackTimer >> [&](const TimerSource &)
  {
    static int cnt = 0;
    cnt++;
    if (_state == CONNECTING)
    {
      if (cnt & 1)
      {
        CborEncoder cborOut(128);
        cborOut.start().write('[').write("pub").write(_loopbackTopic).write(Sys::millis()).write(']').end();
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
      cborOut.start().write('[').write("pub").write(_loopbackTopic).write(Sys::millis()).write(']').end();
      sendCbor(cborOut);
    }
  };

  _connectionWatchdog >> [this](const TimerSource &)
  {
    INFO("WDT connection")
    connected = false;
    _state = CONNECTING;
  };

  subscriber<uint64_t>(_loopbackTopic) >> [&](const uint64_t &in)
  {
    connected = true;
    _state = READY;
    _cborOut.start().write('[').write("pub").write(_latencyTopic).write(Sys::millis() - in).write(']').end();
    sendCbor(_cborOut);
    _connectionWatchdog.start();
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
