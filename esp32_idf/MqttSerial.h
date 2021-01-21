#ifndef MQTTSERIAL_H
#define MQTTSERIAL_H
#undef min
#undef max
#include <Hardware.h>
#include <Mqtt.h>
#include <limero.h>

#include <string>
#include "driver/uart.h"

#define QOS 0
#define TIMEOUT 10000L
#define TIMER_KEEP_ALIVE 1
#define TIMER_CONNECT 2
#define TIMER_SERIAL 3

class MqttSerial : public Mqtt
{
  StaticJsonDocument<1000> _jsonBuffer;
  std::string _clientId;
  std::string _address;
  std::string _lwt_topic;
  std::string _lwt_message;
  UART &_uart;
  StaticJsonDocument<256> txd;
  StaticJsonDocument<256> rxd;
  std::string _rxdString;
  std::string _loopbackTopic;
  uint64_t _loopbackReceived;

  enum
  {
    CMD_SUBSCRIBE = 0,
    CMD_PUBLISH
  };

  void handleSerialByte(uint8_t);
  void rxdSerial(std::string &);
  void txdSerial(JsonDocument &);
  void publish(std::string topic, std::string message);
  void subscribe(std::string topic);

public:
  static void onRxd(void *);

  TimerSource keepAliveTimer;
  TimerSource connectTimer;
  MqttSerial(Thread &thr);
  ~MqttSerial();
  void init();

  void mqttPublish(const char *topic, const char *message);
  void mqttConnect();
  void mqttDisconnect();

  bool handleMqttMessage(const char *message);
  void observeOn(Thread &);

  void on(const TimerMsg &);
  void on(const MqttMessage &);
  void request();
};

#endif // MQTTSERIAL_H
