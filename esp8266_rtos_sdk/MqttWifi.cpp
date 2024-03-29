#include <MqttWifi.h>
#include <Wifi.h>
#include <sys/types.h>
#include <unistd.h>
#include <StringUtility.h>
// SEE :
// https://github.com/espressif/esp-idf/blob/master/examples/protocols/mqtt/tcp/main/app_main.c
// volatile MQTTAsync_token deliveredtoken;

//#define BZERO(x) ::memset(&x, 0, sizeof(x))

#ifndef MQTT_HOST
#error "check MQTT_HOST definition "
#endif
#ifndef MQTT_PORT
#error "check MQTT_PORT definition"
#endif
#define STRINGIFY(X) #X
#define S(X) STRINGIFY(X)

#define TIMER_KEEP_ALIVE 1
#define TIMER_2 2
//________________________________________________________________________
//
MqttWifi::MqttWifi(Thread &thread)
    : Mqtt(thread),
      _reportTimer(thread, 1000, true, "mqtt.report"),
      _keepAliveTimer(thread, 5000,true,"mqtt.keepAlive"), wifiConnected(true)
{
  _lwt_message = "false";
  incoming.async(thread);
}
//________________________________________________________________________
//
MqttWifi::~MqttWifi() {}
//________________________________________________________________________
//
void MqttWifi::init()
{
  address = stringFormat("mqtt://%s:%d", S(MQTT_HOST), MQTT_PORT);
  _lwt_topic = stringFormat("src/%s/system/alive", Sys::hostname());
  srcPrefix = "src/";
  srcPrefix += Sys::hostname();
  srcPrefix += "/";
  dstPrefix = "dst/";
  dstPrefix += Sys::hostname();
  dstPrefix += "/";
  _clientId = Sys::hostname();
  //	esp_log_level_set("*", ESP_LOG_VERBOSE);
  esp_mqtt_client_config_t mqtt_cfg;
  BZERO(mqtt_cfg);
  INFO(" uri : %s ", _address.c_str());

  mqtt_cfg.uri = _address.c_str();
  mqtt_cfg.event_handle = MqttWifi::mqtt_event_handler;
  mqtt_cfg.client_id = Sys::hostname();
  mqtt_cfg.user_context = this;
  mqtt_cfg.buffer_size = 10240;
  mqtt_cfg.lwt_topic = _lwt_topic.c_str();
  mqtt_cfg.lwt_msg = _lwt_message.c_str();
  mqtt_cfg.lwt_qos = 1;
  mqtt_cfg.lwt_msg_len = 5;
  mqtt_cfg.keepalive = 20; // to support OTA blocking
  _mqttClient = esp_mqtt_client_init(&mqtt_cfg);

  _reportTimer.start();

  _reportTimer >>
      ([&](const TimerMsg &tm) { mqttPublish(_lwt_topic.c_str(), "true"); });

  wifiConnected.async(thread());
  wifiConnected >>  [&](bool conn) {
    INFO("WiFi %sconnected.", conn ? "" : "dis");
    if (conn)
    {
      esp_mqtt_client_start(_mqttClient);
    }
    else
    {
      if (connected())
        esp_mqtt_client_stop(_mqttClient);
    }
  };

  outgoing.async(thread());
  outgoing >>  [&](const MqttMessage &m) {
    mqttPublish(m.topic.c_str(), m.message.c_str());
  };

  subscriptions.emplace(dstPrefix + "#");

  _keepAliveTimer.interval(1000);
  _keepAliveTimer.repeat(true);
  _keepAliveTimer >> [&](const TimerMsg &tm) {
    INFO("");
    if (connected())
      outgoing.on({_lwt_topic, "true"});
  };
}
//________________________________________________________________________
//

//________________________________________________________________________
//

void MqttWifi::onNext(const MqttMessage &m)
{
  if (connected())
  {
    mqttPublish(m.topic.c_str(), m.message.c_str());
  };
}
//________________________________________________________________________
//
void MqttWifi::onNext(const TimerMsg &tm)
{
  if (connected())
  {
    onNext({_lwt_topic.c_str(), "true"});
  }
}
//________________________________________________________________________
//
static std::string data;

int MqttWifi::mqtt_event_handler(esp_mqtt_event_t *event)
{
  MqttWifi &me = *(MqttWifi *)event->user_context;
  std::string topics;
  //	esp_mqtt_client_handle_t client = event->client;
  //	int msg_id;

  switch (event->event_id)
  {
  case MQTT_EVENT_BEFORE_CONNECT:
  {
    break;
  }
  case MQTT_EVENT_CONNECTED:
  {
    INFO("MQTT_EVENT_CONNECTED to %s", me._address.c_str());
    INFO(" session : %d %d ", event->session_present, event->msg_id);
    esp_mqtt_client_publish(me._mqttClient, "src/limero/systems",
                            Sys::hostname(), 0, 1, 0);
    INFO("publish done ");
    for (auto subscription : me.subscriptions)
      me.mqttSubscribe(subscription);
    me.connected = true;
    break;
  }
  case MQTT_EVENT_DISCONNECTED:
  {
    INFO("MQTT_EVENT_DISCONNECTED");
    me.connected = false;
    break;
  }
  case MQTT_EVENT_SUBSCRIBED:
    INFO("MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    INFO("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_PUBLISHED:
    //			INFO("MQTT_EVENT_PUBLISHED, msg_id=%d",
    // event->msg_id);
    break;
  case MQTT_EVENT_DATA:
  {
    //    INFO("MQTT_EVENT_DATA 0x%X 0x%X ",event,event->user_context);
    if (event->current_data_offset == 0)
    {
      me._lastTopic = std::string(event->topic, event->topic_len);
    }
    bool isOtaData = me._lastTopic.find("/ota") != std::string::npos;
    /*        INFO(" MQTT_EVENT_DATA %s offset:%d length:%d total:%d ",
                me._lastTopic.c_str(), event->current_data_offset,
         event->data_len, event->total_data_len);*/
    if (isOtaData)
    {
      MqttBlock block;
      block.offset = event->current_data_offset;
      block.length = event->data_len;
      block.total = event->total_data_len;
      block.topic = me._lastTopic;
      block.data = (uint8_t *)event->data;
      me.blocks.on(block);
    }
    else
    {
      if (event->current_data_offset == 0)
      {
        data = std::string(event->data, event->data_len);
      }
      else
      {
        data.append(event->data, event->data_len);
      }
      if (event->current_data_offset + event->data_len ==
          event->total_data_len)
      {
        INFO("MQTT RXD topic : %s , message  : %s", me._lastTopic.c_str(), data.c_str());
        me.incoming.on({me._lastTopic, data});
      }
    }
    break;
  }
  case MQTT_EVENT_ERROR:
    WARN("MQTT_EVENT_ERROR");
    break;
  default:
  {
    WARN(" unknown MQTT event : %d ", event->event_id);
  }
  }
  return ESP_OK;
}
//________________________________________________________________________
//
typedef enum
{
  PING = 0,
  PUBLISH,
  PUBACK,
  SUBSCRIBE,
  SUBACK
} CMD;
//________________________________________________________________________
//
void MqttWifi::mqttPublish(const char *topic, const char *message)
{
  if (connected() == false)
    return;
  INFO("PUB : %s = %s", topic, message);
  int id = esp_mqtt_client_publish(_mqttClient, topic, message, 0, 0, 0);
  if (id < 0)
    WARN("esp_mqtt_client_publish() failed.");
}
//________________________________________________________________________
//
void MqttWifi::mqttSubscribe(std::string& topic)
{
  INFO("Subscribing to topic %s.", topic.c_str());
  int id = esp_mqtt_client_subscribe(_mqttClient, topic.c_str(), 1);
  if (id < 0)
    WARN("esp_mqtt_client_subscribe() failed.");
}
