#ifndef MQTTSERIAL_H
#define MQTTSERIAL_H
#undef min
#undef max
#include <Hardware.h>
#include <Mqtt.h>
#include <limero.h>
#include <Usb.h>
#include <ArduinoJson.h>

#include <string>


#define QOS 0
#define TIMEOUT 10000L
#define TIMER_KEEP_ALIVE 1
#define TIMER_CONNECT 2
#define TIMER_SERIAL 3

class MqttSerial : public Mqtt {
  StaticJsonDocument<3000> _jsonBuffer;
  std::string  _clientId;
  std::string  _address;
  std::string  _lwt_topic;
  std::string  _lwt_message;

 protected:
  StaticJsonDocument<256> txd;
  StaticJsonDocument<256> rxd;
  std::string  _rxdString;
  std::string  _loopbackTopic;
  uint64_t _loopbackReceived;

  enum { CMD_SUBSCRIBE = 0, CMD_PUBLISH };

  void handleSerialByte(uint8_t);
  void rxdSerial(const std::string  &);
  void txdSerial();
  void publish(std::string  topic, std::string  message);
  void subscribe(std::string topic);

 public:
  ValueSource<bool> connected;
  TimerSource keepAliveTimer;
  TimerSource connectTimer;
  Sink<std::string> rxdLine;
  ValueSource<std::string> txdLine;
  MqttSerial(Thread &thr);
  ~MqttSerial();
  void init();

  void mqttPublish(const char *topic, const char *message);
  //void mqttSubscribe(const char *topic);
  void mqttConnect();
  void mqttDisconnect();

  bool handleMqttMessage(const char *message);
  void observeOn(Thread &);

  void on(const TimerMsg &);
  void on(const MqttMessage &);
  void request();
};

#endif  // MQTTSERIAL_H
