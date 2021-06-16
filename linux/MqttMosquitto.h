#ifndef _MQTT_MOSQUITTO_H_
#define _MQTT_MOSQUITTO_H_
#include <Mqtt.h>
#include <limero.h>

#include "mosquitto.h"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <ArduinoJson.h>

#define QOS 0
#define TIMEOUT 10000L
#define MQTT_HOST "test.mosquitto.org"
#define MQTT_PORT 1883

class MqttMosquitto : public Mqtt
{
public:
  typedef enum
  {
    MS_DISCONNECTED,
    MS_CONNECTING,
    MS_CONNECTED
  } MqttConnectionState;

private:
  std::string _clientId;
  TimerSource _reportTimer;
  TimerSource _keepAliveTimer;

  struct mosquitto *_client;
  std::string _connection;
  std::string _host="localhost";
  uint16_t _port=1883;
  uint32_t _keepalive=60;
  int _keepAliveInterval;

  MqttConnectionState _connectionState;
  void state(MqttConnectionState newState, const char *file, int line);
  std::string _lastWillTopic;
  std::string _lastWillMessage;
  int _lastWillQos;
  bool _lastWillRetain;

  static void onLog(struct mosquitto *mosq, void *userdata, int level,
                    const char *str);
  static void onMessage(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
  static void onSubscribe(struct mosquitto *mosq,  void *, int, int, const int *);
  static void onDisconnect(struct mosquitto *mosq,  void *, int);
  /*
  static void onConnectionLost(void *context, char *cause);
  static int onMessage(void *context, char *topicName, int topicLen,
                       MQTTAsync_message *message);
  
  static void onConnectFailure(void *context, MQTTAsync_failureData *response);
  static void onConnectSuccess(void *context, MQTTAsync_successData *response);
  static void onSubscribeSuccess(void *context,
                                 MQTTAsync_successData *response);
  static void onSubscribeFailure(void *context,
                                 MQTTAsync_failureData *response);
  static void onPublishSuccess(void *context, MQTTAsync_successData *response);
  static void onPublishFailure(void *context, MQTTAsync_failureData *response);
  static void onDeliveryComplete(void *context, MQTTAsync_token response);*/

public:
  MqttMosquitto(Thread &thread);
  ~MqttMosquitto();
  void init();
  void config(JsonObject &);
  int connection(std::string);
  int client(std::string);
  int connect();
  int disconnect();
  int publish(std::string topic, std::string message, int qos = 0,
              bool retain = false);
  int subscribe(std::string topic);
  int lastWill(std::string topic, std::string message, int qos, bool retain);
  MqttConnectionState state();
};

//_______________________________________________________________________________________________________________
//

#endif
