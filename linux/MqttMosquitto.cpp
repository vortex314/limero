#include <Config.h>
#include <MqttMosquitto.h>
#include <sys/types.h>
#include <unistd.h>
// SEE :
// https://github.com/espressif/esp-idf/blob/master/examples/protocols/mqtt/tcp/main/app_main.c
// volatile MQTTAsync_token deliveredtoken;
const char *mqttConnectionStates[] = {"MS_DISCONNECTED",
                                      "MS_CONNECTING", "MS_CONNECTED"};
//#define BZERO(x) ::memset(&x, 0, sizeof(x))

#define STRINGIFY(X) #X
#define S(X) STRINGIFY(X)

#define TIMER_KEEP_ALIVE 1
#define TIMER_2 2

const char *logMosquitto(int rc)
{
  switch (rc)
  {
  case MOSQ_ERR_SUCCESS:
    return "success";
  case MOSQ_ERR_INVAL:
    return "the input parameters were invalid.";
  case MOSQ_ERR_NOMEM:
    return "	an out of memory condition occurred.";
  case MOSQ_ERR_PAYLOAD_SIZE:
    return " payloadlen is too large.";
  case MOSQ_ERR_MALFORMED_UTF8:
    return " the topic is not valid UTF-8.";
  case MOSQ_ERR_NO_CONN:
    return "no connection.";
  case MOSQ_ERR_OVERSIZE_PACKET:
    "if the resulting packet would be larger than supported by the broker.";
  default:
    return "undocumented error";
  }
}

//________________________________________________________________________
//
MqttMosquitto::MqttMosquitto(Thread &thread)
    : Mqtt(thread), _reportTimer(thread, 1000, true), _keepAliveTimer(thread, 3000, true)
{
  incoming.async(thread);
  _connectionState = MS_DISCONNECTED;
  int major;
  int minor;
  int revision;
  int rc = mosquitto_lib_version(&major, &minor, &revision);
  INFO(" libmosquitto : %d.%d.%d", major, minor, revision);
  mosquitto_lib_init();
  
}
//________________________________________________________________________
//
MqttMosquitto::~MqttMosquitto() {}

void MqttMosquitto::config(JsonObject &conf)
{
  _connection = conf["connection"] | "tcp://test.mosquitto.org";
  _lastWillMessage = conf["LW"]["message"] | "false";
  _lastWillQos = conf["LW"]["qos"] | 0;
  srcPrefix = "src/";
  srcPrefix += conf["device"] | Sys::hostname();
  srcPrefix += "/";
  dstPrefix = "dst/";
  dstPrefix += conf["device"] | Sys::hostname();
  dstPrefix += "/";
  std::string defaultTopic = srcPrefix + "system/alive";
  _lastWillTopic = conf["LW"]["topic"] | defaultTopic;
  _lastWillRetain = conf["LW"]["retain"] | false;
  _keepAliveInterval = conf["keepAliveInterval"] | 20;
};
//________________________________________________________________________
//
void MqttMosquitto::init()
{
  _clientId = dstPrefix;
  INFO(" mqtt mosquitto as : %s ",dstPrefix.c_str() );
    bool clean_session = true;
  _client = mosquitto_new(dstPrefix.c_str(), clean_session, this);
  if (!_client)
  {
    WARN("Error: Out of memory.\n");
    exit(1);
  }
  INFO(" init() connection : %s ", _connection.c_str());
  state(MS_DISCONNECTED, __SHORT_FILE__, __LINE__);

  _reportTimer >>
      ([&](const TimerMsg &) { publish(_lastWillTopic.c_str(), "true"); });

  outgoing.async(thread(), [&](const MqttMessage &m) {
    publish(m.topic.c_str(), m.message.c_str());
  });

  _keepAliveTimer >> [&](const TimerMsg &) {
    if (!connected())
      connect();
  };
  subscriptions.emplace(dstPrefix + "#");
}
//________________________________________________________________________
//

int MqttMosquitto::lastWill(std::string topic, std::string message, int qos,
                            bool retain)
{
  _lastWillMessage = message;
  _lastWillQos = qos;
  _lastWillTopic = topic;
  _lastWillRetain = retain;
  return 0;
}

int MqttMosquitto::client(std::string cl)
{
  _clientId = cl;
  return 0;
}

void MqttMosquitto::state(MqttConnectionState newState, const char *file, int line)
{
  INFO(" MQTT connection state %s => %s from %s:%d",
       mqttConnectionStates[_connectionState], mqttConnectionStates[newState], file, line);

  _connectionState = newState;
  connected = newState == MS_CONNECTED;
}

MqttMosquitto::MqttConnectionState MqttMosquitto::state() { return _connectionState; }

void MqttMosquitto::onLog(struct mosquitto *mosq, void *userdata, int level,
                          const char *str)
{
  /* Pring all log messages regardless of level. */

  switch (level)
  {
  // case MOSQ_LOG_DEBUG:
  // case MOSQ_LOG_INFO:
  // case MOSQ_LOG_NOTICE:
  case MOSQ_LOG_WARNING:
  case MOSQ_LOG_ERR:
  {
    INFO("%i:%s\n", level, str);
  }
  }
}

int MqttMosquitto::connect()
{
  int rc;
  INFO(" mqtt mosquitto as : %s ",dstPrefix.c_str() );

  if (_connectionState == MS_CONNECTING || _connectionState == MS_CONNECTED)
  {
    ERROR(" not in DISCONNECTED state ");
    return ECONNREFUSED;
  }

  INFO(" MQTT connecting %s ...  ", _connection.c_str());
  state(MS_CONNECTING, __SHORT_FILE__, __LINE__);
  mosquitto_log_callback_set(_client, onLog);
  mosquitto_message_callback_set(_client, onMessage);
  mosquitto_subscribe_callback_set(_client, onSubscribe);
  /*rc = mosquitto_int_option(_client, MOSQ_OPT_TCP_NODELAY, 1);
  if (rc)
    WARN("mosquitto_int_option() : %s  ", logMosquitto(rc));
    */
  rc = mosquitto_connect(_client, _host.c_str(), _port, _keepalive);
  if (rc)
  {
    WARN("Unable to connect : %s ", logMosquitto(rc));
    return rc;
  };
  INFO(" MQTT mosquitto connected.");
  rc = mosquitto_will_set(_client, _lastWillTopic.c_str(), _lastWillMessage.length(), _lastWillMessage.c_str(), _lastWillQos, _lastWillRetain);
  if (rc)
    WARN("mosquitto_will_set() : %s  ", logMosquitto(rc));
  state(MS_CONNECTED, __SHORT_FILE__, __LINE__);
  for (auto &subscription : subscriptions)
    subscribe(subscription);
  int loop = mosquitto_loop_start(_client);
  if (loop != MOSQ_ERR_SUCCESS)
  {
    WARN("Unable to start loop: %i", loop);
    exit(1);
  }
  return 0;
}

int MqttMosquitto::disconnect()
{
  mosquitto_disconnect(_client);
  state(MS_DISCONNECTED, __SHORT_FILE__, __LINE__);
  return 0;
}

int MqttMosquitto::subscribe(std::string topic)
{

  int qos = 0;
  subscriptions.emplace(topic);
  if (state() != MS_CONNECTED)
    return 0;
  INFO("Subscribing to topic %s for client %s using QoS%d", topic.c_str(),
       _clientId.c_str(), 0);
  int rc = mosquitto_subscribe(_client, NULL, topic.c_str(), 0);
  if (rc)
    WARN("mosquitto_subscribe() : %s  ", logMosquitto(rc));
  INFO(" subscribe success ");

  return 0;
}
/*
void MqttMosquitto::onConnectionLost(void *context, char *cause)
{
  MqttMosquitto *me = (MqttMosquitto *)context;
  me->state(MS_DISCONNECTED, __SHORT_FILE__, __LINE__);
  INFO(" MQTT connection lost : %s", cause);
}*/

void MqttMosquitto::onMessage(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
  DEBUG("got message '%.*s' for topic '%s'", message->payloadlen, (char *)message->payload, message->topic);
  MqttMosquitto *me = (MqttMosquitto *)obj;
  std::string msg((char *)message->payload, message->payloadlen);
  std::string topic = message->topic;
  me->incoming.on({topic, msg});
}

void MqttMosquitto::onSubscribe(struct mosquitto *, void *context, int messageId, int qosCount, const int *grantedQos)
{
  MqttMosquitto *me = (MqttMosquitto *)context;
  INFO("MQTT_EVENT_SUBSCRIBED, msg_id=%d qosCount:%d grantedQos:%d", messageId, qosCount, *grantedQos);
}
/*
void MqttMosquitto::onDeliveryComplete(void *context, MQTTAsync_token)
{
  //    MqttMosquitto* me = (MqttMosquitto*)context;
}
*/
void MqttMosquitto::onDisconnect(struct mosquitto *mosq,  void *context, int rc)
{
  MqttMosquitto *me = (MqttMosquitto *)context;
  me->state(MS_DISCONNECTED, __SHORT_FILE__, __LINE__);
  INFO(" MQTT disconnected : %s ",logMosquitto(rc));
}
/*
void MqttMosquitto::onConnectFailure(void *context,
                                     MQTTAsync_failureData *)
{
  MqttMosquitto *me = (MqttMosquitto *)context;
  me->state(MS_DISCONNECTED, __SHORT_FILE__, __LINE__);
}

void MqttMosquitto::onConnectSuccess(void *context,
                                     MQTTAsync_successData *)
{
  MqttMosquitto *me = (MqttMosquitto *)context;
  me->state(MS_CONNECTED, __SHORT_FILE__, __LINE__);
  for (auto &subscription : me->subscriptions)
    me->subscribe(subscription);
}



void MqttMosquitto::onSubscribeFailure(void *context,
                                       MQTTAsync_failureData *response)
{
  MqttMosquitto *me = (MqttMosquitto *)context;
}
*/
int MqttMosquitto::publish(std::string topic, std::string message, int qos,
                           bool retained)
{

  //  INFO(" MQTT PUB : %s = %s %s", topic.c_str(), message.c_str(),connected()?"CONNECTED":"DISCONNECTED");
  if (!connected())
    return ECOMM;
  int rc = mosquitto_publish(_client, NULL, topic.c_str(), message.length(), message.c_str(), 0, 0);
  if (rc)
    WARN("mosquitto_publish() : %s  ", logMosquitto(rc));
  return 0;
}
/*
void MqttMosquitto::onPublishSuccess(void *context,
                                     MQTTAsync_successData *response)
{
  MqttMosquitto *me = (MqttMosquitto *)context;
}
void MqttMosquitto::onPublishFailure(void *context,
                                     MQTTAsync_failureData *response)
{
  MqttMosquitto *me = (MqttMosquitto *)context;
  me->disconnect();
}
*/
