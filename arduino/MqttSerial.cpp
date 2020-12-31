
#include <MqttSerial.h>

MqttSerial::MqttSerial(Thread &thr, HardwareSerial &serial)
    : Mqtt(thr), _serial(serial),
      connected(false),
      keepAliveTimer(thr, 1000, true),
      connectTimer(thr, 1000, true)
{
  _rxdString.reserve(256);
  incoming.async(thr);
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

  _lwt_topic = srcPrefix + "system/alive";
  _loopbackTopic += dstPrefix + "system/loopback";
  _loopbackReceived = 0;

  outgoing.async(thread(), [&](const MqttMessage &m) {
    if (connected())
    {
      publish(m.topic, m.message);
    }
  });

  // Sink<TimerMsg, 3> &me = *this;
  keepAliveTimer >> [&](const TimerMsg &tm) {
    if (connected())
      outgoing.on({_lwt_topic, "true"});
  };
  connectTimer >> [&](const TimerMsg &tm) {
    INFO(" connectTimer ");
    if (Sys::millis() > (_loopbackReceived + 3000))
    {
      connected = false;
      subscribe(dstPrefix + "#");
      publish(_loopbackTopic, "true");
    }
    else
    {
      connected = true;
    }
    publish(_loopbackTopic, "true");
  };
  _serial.setTimeout(0);
}

void MqttSerial::request() {}

void MqttSerial::onRxd(void *me)
{
  MqttSerial *mqttSerial = (MqttSerial *)me;
  String s;
  while (mqttSerial->_serial.available() ){
    char inChar = ( char) mqttSerial->_serial.read();
    s+= inChar;
  }
 // INFO(" RXD >> %d", s.length());
  for (uint32_t i = 0; i < s.length(); i++)
    mqttSerial->handleSerialByte(s.charAt(i));
}

void MqttSerial::handleSerialByte(uint8_t b)
{
  if (b == '\r' || b == '\n')
  {
    if (_rxdString.length() > 0)
    {
      rxdSerial(_rxdString);
    }
    _rxdString = "";
  }
  else
  {
    _rxdString += (char)b;
  }
}

void MqttSerial::rxdSerial(std::string &rxdString)
{
  deserializeJson(rxd, rxdString);
  JsonArray array = rxd.as<JsonArray>();
  if (!array.isNull())
  {
    if (array[1].as<std::string>() == _loopbackTopic)
    {
      _loopbackReceived = Sys::millis();
      connected = true;
      std::string topic = array[1];
      std::string arg = array[2];
      INFO(" RXD >>> %s : %s ", topic.c_str(), arg.c_str());
    }
    else
    {
      std::string topic = array[1];
      std::string arg = array[2];
      INFO(" RXD >>> %s : %s ", topic.c_str(), arg.c_str());
      incoming.on({topic, arg});
    }
  }
  else
  {
    WARN(" parsing JSON array failed ");
  }
}

void MqttSerial::publish(std::string topic, std::string message)
{
  txd.clear();
  txd.add((int)CMD_PUBLISH);
  txd.add(topic);
  txd.add(message);
  txdSerial(txd);
}

void MqttSerial::subscribe(std::string topic)
{
  txd.clear();
  txd.add((int)CMD_SUBSCRIBE);
  txd.add(topic);
  txdSerial(txd);
}

void MqttSerial::txdSerial(JsonDocument &txd)
{
  String output = "";
  serializeJson(txd, output);
  Serial.println(output.c_str());
  Serial.flush();
}