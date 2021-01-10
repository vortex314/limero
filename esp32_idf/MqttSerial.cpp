#include <MqttSerial.h>

MqttSerial::MqttSerial(Thread &thr)
    : Mqtt(thr),
      _uart(UART::create(UART_NUM_0, 1, 3)),
      connected(false),
      keepAliveTimer(thr, 500, true, "keepAlive"),
      connectTimer(thr, 3000, true, "connect")
{
  _rxdString.reserve(256);
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

  _loopbackTopic +=  dstPrefix + "system/loopback";
  _loopbackReceived = 0;

  outgoing.async(thread(), [&](const MqttMessage &m) {
    if (connected())
    {
      publish(m.topic, m.message);
    }
  });

  keepAliveTimer >> [&](const TimerMsg &tm) {
    std::string s="true";
    publish(_loopbackTopic, s);
    outgoing.on({srcPrefix+"system/alive", s});
  };
  connectTimer >> [&](const TimerMsg &tm) {
    if (Sys::millis() > (_loopbackReceived + 2000))
    {
      connected = false;
      std::string topic;
      string_format(topic, "dst/%s/#", Sys::hostname());
      for (auto &subscription : subscriptions)
        subscribe(subscription);
      publish(_loopbackTopic, "true");
    }
    else
    {
      connected = true;
    }
  };
  _uart.setClock(115200);
  _uart.onRxd(onRxd, this);
  _uart.mode("8N1");
  _uart.init();
  subscriptions.emplace(dstPrefix + "#");
}

void MqttSerial::request() {}

void MqttSerial::onRxd(void *me)
{
  static std::string bytes;
  MqttSerial *mqttSerial = (MqttSerial *)me;
  while (mqttSerial->_uart.hasData())
  {
    bytes.clear();
    mqttSerial->_uart.read(bytes);
    for (int i = 0; i < bytes.length(); i++)
      mqttSerial->handleSerialByte(bytes[i]);
  }
}

void MqttSerial::handleSerialByte(uint8_t b)
{
  if (b == '\r' || b == '\n')
  {
    if (_rxdString.length() > 0)
    {
      DEBUG(" RXD : %s ", _rxdString.c_str());
      rxdSerial(_rxdString);
    }
    _rxdString.clear();
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
    }
    else
    {
      incoming.on({array[1], array[2]});
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
  std::string output = "";
  serializeJson(txd, output);
  printf("%s\n", output.c_str());
}
