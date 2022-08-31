/*
 * Spine.cpp
 *
 *  Created on: Nov 21, 2021
 *      Author: lieven
 */

#include "RedisSpineCbor.h"
#include "StringUtility.h"

RedisSpineCbor::RedisSpineCbor(Thread &thr)
    : Actor(thr),
      _cborOut(MAX_SIZE),
      _cborIn(MAX_SIZE),
      _loopbackTimer(thr, 950, true, "loopbackTimer"),
      _connectionWatchdog(thr, 6000, true, "connectTimer"),
      rxdFrame(5, "Redis:rxdFrame"),
      txdFrame(5, "Redis:txdFrame")
{
  setNode(Sys::hostname());
  _state = CONNECTING;
  connected = false;
  rxdFrame.async(thread());
  txdFrame.async(thread());
  rxdFrame >> [&](const Bytes &bs)
  {
    _cborIn.clear();
    _cborIn.put_bytes(bs.data(), bs.size());
    messageArrived.on(true);
  };

  messageArrived >> [&](bool)
  {
    std::string cmd;
    if (_cborIn.rewind().read('[').read("pub").ok())
    {
      pubArrived.on(true);
    }
  };

  _loopbackTimer >> [&](const TimerMsg &tm)
  {
    static int cnt = 0;
    cnt++;
    if (_state == CONNECTING)
    {
      if (cnt & 1)
      {
        ProtocolEncoder cborOut(128);
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
      ProtocolEncoder cborOut(128);
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
  _subscribePattern = dstPrefix + "*";
  connected = false;
}

void RedisSpineCbor::sendCbor(ProtocolEncoder &out)
{
  txdFrame.on(out);
}

void RedisSpineCbor::subscribe(const char *pattern)
{
  _cborOut.start().write('[').write("sub").write(pattern).write(']').end();
  sendCbor(_cborOut);
}
