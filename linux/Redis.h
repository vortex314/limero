#ifndef __REDIS_H__
#define __REDIS_H__
#include <ArduinoJson.h>
#include <async.h>
#include <hiredis.h>
#include <limero.h>

void replyToJson(JsonVariant, redisReply*);
bool validate(JsonVariant js, std::string format);
int token(JsonVariant v);

class Json : public DynamicJsonDocument {
 public:
  Json() : DynamicJsonDocument(10240) {}
  Json(int x) : DynamicJsonDocument(x){};
  Json(DynamicJsonDocument jsd) : DynamicJsonDocument(jsd){}
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

 public:
  Redis(Thread &thread, JsonObject config);
  ~Redis();

  static void onPush(redisAsyncContext *ac, void *reply);
  static void replyHandler(redisAsyncContext *ac, void *reply, void *me);
  static void free_privdata(void *pvdata);

  int connect();
  void disconnect();
  void stop();

  Sink<Json> &request();
  Sink<std::string>& command();
  Source<Json> &response();
  Source<bool> &connected();

  static void addWriteFd(void *pv);
  static void addReadFd(void *pv);
  static void delWriteFd(void *pv);
  static void delReadFd(void *pv);
  static void cleanupFd(void *pv);
};
#endif