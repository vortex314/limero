#ifndef __REDIS_H__
#define __REDIS_H__
#include <Json.h>
#include <async.h>
#include <hiredis.h>
#include <limero.h>

void redisReplyToJson(JsonVariant, redisReply *);
bool validate(JsonVariant js, std::string format);
int token(JsonVariant v);

class Redis;

struct RedisReplyContext {
  std::string command;
  Redis *me;
  RedisReplyContext(const std::string &command, Redis *me)
      : command(command), me(me) {
    INFO("new RedisReplyContext %X : %s", this,command.c_str());
  }
  ~RedisReplyContext() { INFO("delete RedisReplyContext %X", this); }
};

class Redis : public Actor {
  QueueFlow<Json> _request;
  QueueFlow<Json> _response;
  ValueFlow<std::string> _command;
  redisAsyncContext *_ac;
  ValueFlow<bool> _connected;
  std::string _redisHost;
  uint16_t _redisPort;
  Json _docIn;
  bool _reconnectOnConnectionLoss;
  bool _addReplyContext;
  SinkFunction<Json> *_jsonToRedis;
  std::vector<std::string> _ignoreReplies;
  std::vector<std::string> _subscribedChannels;
  enum { CS_DISCONNECTED, CS_CONNECTED, CS_CONNECTING } _connectionStatus;
  TimerSource _connectionTimer;
  std::vector<std::string> _initCommands;

 public:
  Redis(Thread &thread, JsonObject config);
  ~Redis();

  static void onPush(redisAsyncContext *ac, void *reply);
  static void replyHandler(redisAsyncContext *ac, void *reply, void *me);
  void makeEnvelope(JsonVariant envelope, RedisReplyContext *redisReplyContext);

  static void free_privdata(void *pvdata);

  int connect();
  void disconnect();
  void reconnect();
  void stop();
  void publish(std::string channel, std::string message);
  void subscribe(std::string channel);

  Sink<Json> &request();
  Sink<std::string> &command();
  Source<Json> &response();
  Source<bool> &connected();

  void responseFailure(int rc, std::string message);

  static void addWriteFd(void *pv);
  static void addReadFd(void *pv);
  static void delWriteFd(void *pv);
  static void delReadFd(void *pv);
  static void cleanupFd(void *pv);

  template <typename T>
  Source<T> &subscriber(std::string pattern) {
    subscribe(pattern);
    auto lf = new LambdaFlow<Json, T>([&, pattern](T &t, const Json &msg) {
      std::string s;
      serializeJson(msg, s);
      if (msg[0] == "pmessage" && msg[1] == pattern) {
        DynamicJsonDocument doc(1024);
        auto rc = deserializeJson(doc, msg[3].as<std::string>());
        if (rc == DeserializationError::Ok && doc.is<T>()) {
          t = doc.as<T>();
          return true;
        }
      }
      return false;
    });
    _response >> lf;
    return *lf;
  }
  template <typename T>
  Sink<T> &publisher(std::string topic) {
    SinkFunction<T> *sf = new SinkFunction<T>([&, topic](const T &t) {
      Json json;
      std::string s;
      Json valueJson(1024);
      valueJson.as<JsonVariant>().set(t);
      serializeJson(valueJson, s);
      publish(topic, s);
    });
    return *sf;
  }
};
#endif