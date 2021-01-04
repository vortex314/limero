#include <MqttSerial.h>

MqttSerial::MqttSerial(Thread &thr)
    : Mqtt(thr),
      connected(false),
      keepAliveTimer(thr, 10, true, "keepAlive"),
      connectTimer(thr, 100, true, "connect"),
      rxdLine(4)
{
  _rxdString.reserve(256);
  rxdLine.async(thr, [&](const std::string &s) { rxdSerial(s); });
}
MqttSerial::~MqttSerial() {}

void MqttSerial::init()
{
  INFO("MqttSerial started. ");
  txd.clear();
  rxd.clear();
  srcPrefix = "src/";
  srcPrefix += Sys::hostname();
  srcPrefix += "/";
  dstPrefix = "dst/";
  dstPrefix += Sys::hostname();
  dstPrefix += "/";

  _loopbackTopic += dstPrefix + "system/loopback";
  _loopbackReceived = 0;

  outgoing.async(thread(), [&](const MqttMessage &m) {
    if (connected())
    {
      publish(m.topic, m.message);
    }
  });

  keepAliveTimer >> [&](const TimerMsg &) {
    publish(_loopbackTopic, std::string("true"));
    outgoing.on({srcPrefix + "system/alive", "true"});
  };
  connectTimer >> [&](const TimerMsg &) {
    if (Sys::millis() > (_loopbackReceived + 2000))
    {
      connected = false;
      for (auto &subscription : subscriptions)
        subscribe(subscription);
      publish(_loopbackTopic, "true");
    }
    else
    {
      connected = true;
    }
  };
  subscriptions.emplace(dstPrefix + "#");
}

void MqttSerial::request() {}

void MqttSerial::rxdSerial(const std::string &rxdString)
{
  deserializeJson(rxd, rxdString);
  JsonArray array = rxd.as<JsonArray>();
  if (!array.isNull())
  {
    if (array[1].as<std::string>() == _loopbackTopic)
    {
      _loopbackReceived = Sys::millis();
      connected = true;
    }
    else
    {
      incoming.on({array[1], array[2]});
    }
  }
  else
  {
    WARN(" parsing JSON array failed [%d] : '%s'  ", rxdString.length(), rxdString.c_str());
  }
}

void MqttSerial::publish(std::string pubTopic, std::string pubMessage)
{
  txd.clear();
  txd.add((int)CMD_PUBLISH);
  txd.add(pubTopic);
  txd.add(pubMessage);
  txdSerial();
}

void MqttSerial::subscribe(std::string subTopic)
{
  txd.clear();
  txd.add((int)CMD_SUBSCRIBE);
  txd.add(subTopic);
  txdSerial();
}

void MqttSerial::txdSerial()
{
  std::string output = "";
  serializeJson(txd, output);
  txdLine = output;
}
