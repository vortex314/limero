#ifndef MQTT_ABSTRACT_H
#define MQTT_ABSTRACT_H
#include <limero.h>
#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_ENABLE_STD_STRING 1
#include <ArduinoJson.h>
#include <set>

class Mqtt;
template <class T> class ToMqtt;
template <class T> class FromMqtt;
template <class T> class MqttFlow;

struct MqttMessage {
  std::string topic;
  std::string message;
};

class MqttBlock {
public:
  std::string topic;
  //  Bytes data;
  uint32_t offset;
  uint32_t length;
  uint32_t total;
  uint8_t *data;
  MqttBlock operator=(const MqttBlock &other) {
    topic = other.topic;
    //    data=other.data;
    offset = other.offset;
    length = other.length;
    total = other.total;
    data = other.data;
    return *this;
  }
};

//____________________________________________________________________________________________________________
//
class Mqtt : public Actor {
public:
  std::string dstPrefix;
  std::string srcPrefix;
  std::set<std::string> subscriptions;
  QueueFlow<MqttMessage> incoming;
  Sink<MqttMessage> outgoing;
  ValueFlow<MqttBlock> blocks;
  ValueSource<bool> connected;
  //  TimerSource keepAliveTimer;
  Mqtt(Thread &thr)
      : Actor(thr), incoming(4, "incoming"), outgoing(4, "outgoing") {
    dstPrefix = "dst/";
    dstPrefix += Sys::hostname();
    dstPrefix += "/";
    srcPrefix = "src";
    srcPrefix += Sys::hostname();
    srcPrefix += "/";
  };
  ~Mqtt(){};
  void init();
  template <class T> Subscriber<T> &toTopic(const char *name) {
    auto flow = new ToMqtt<T>(name, this);
    *flow >> outgoing;
    return *flow;
  }
  template <class T> Source<T> &fromTopic(const char *name) {
    auto newSource = new FromMqtt<T>(name, this);
    incoming >> *newSource;
    return *newSource;
  }
  template <class T> Flow<T, T> &topic(const char *name) {
    auto flow = new MqttFlow<T>(name, this);
    incoming >> flow->fromMqtt;
    flow->toMqtt >> outgoing;
    return *flow;
  }
};

//____________________________________________________________________________________________________________
//
template <class T> class ToMqtt : public LambdaFlow<T, MqttMessage> {
  std::string _name;
  Mqtt *_mqtt;

public:
  ToMqtt(std::string name, Mqtt *mqtt)
      : LambdaFlow<T, MqttMessage>([&](MqttMessage &msg, const T &event) {
          if (!_mqtt->connected())
            return ENOTCONN;
          DynamicJsonDocument doc(100);
          JsonVariant variant = doc.to<JsonVariant>();
          variant.set(event);
          serializeJson(doc, msg.message);
          msg.topic = _name;
//          INFO("%s", msg.topic.c_str());
          return 0;
        }),
        _name(name) {
    _mqtt = mqtt;
    std::string topic = name;
    if (topic.find("src/") == 0 || topic.find("dst/") == 0) {
      INFO(" no prefix for %s ", name.c_str());
      _name = name;
    } else {
      INFO(" adding prefix %s to %s ", mqtt->srcPrefix.c_str(), name.c_str());
      _name = mqtt->srcPrefix + name;
    }
  }
  void request(){};
};
//_______________________________________________________________________________________________________________
//
template <class T> class FromMqtt : public LambdaFlow<MqttMessage, T> {
  std::string _name;

public:
  FromMqtt(std::string name, Mqtt *mqtt)
      : LambdaFlow<MqttMessage, T>([&](T &t, const MqttMessage &mqttMessage) {
         // INFO(" from topic '%s':'%s' vs '%s'", mqttMessage.topic.c_str(),
           //    mqttMessage.message.c_str(), _name.c_str());
          if (mqttMessage.topic != _name) {
            return EINVAL;
          }
          DynamicJsonDocument doc(100);
          auto error = deserializeJson(doc, mqttMessage.message);
          if (error) {
            WARN(" failed JSON parsing '%s' : '%s' ",
                 mqttMessage.message.c_str(), error.c_str());
            return ENODATA;
          }
          JsonVariant variant = doc.as<JsonVariant>();
          if (variant.isNull()) {
            WARN(" is not a JSON variant '%s' ", mqttMessage.message.c_str());
            return ENODATA;
          }
          if (variant.is<T>() == false) {
            WARN(" message '%s' JSON type doesn't match.",
                 mqttMessage.message.c_str());
            return ENODATA;
          }
          t = variant.as<T>();
          return 0;
          // emit doesn't work as such
          // https://stackoverflow.com/questions/9941987/there-are-no-arguments-that-depend-on-a-template-parameter
        }) {

    std::string topic = name;
    std::string targetTopic;
    if (topic.find("src/") == 0 || topic.find("dst/") == 0) {
      INFO(" no prefix for %s ", name.c_str());
      _name = name;
      mqtt->subscriptions.emplace(_name); // add explicit subscription beside the implicit src/device/#
    } else {
      INFO(" adding prefix %s to %s ", mqtt->dstPrefix.c_str(), name.c_str());
      _name = mqtt->dstPrefix + name;
    }
  };
  void request(){};
};

//____________________________________________________________________________________________________________
//
template <class T> class MqttFlow : public Flow<T, T> {
public:
  ToMqtt<T> toMqtt;
  FromMqtt<T> fromMqtt;
  MqttFlow(const char *topic, Mqtt *mqtt)
      : toMqtt(topic, mqtt), fromMqtt(topic, mqtt){

                                 //       INFO(" created MqttFlow : %s ",topic);
                             };
  void request() { fromMqtt.request(); };
  void on(const T &t) { toMqtt.on(t); }
  void subscribe(Subscriber<T> *tl) { fromMqtt.subscribe(tl); };
};
#endif
