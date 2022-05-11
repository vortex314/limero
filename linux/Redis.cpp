#include <Redis.h>
#include <assert.h>

#include <algorithm>

struct RedisReplyContext {
  std::string command;
  Redis *me;
  RedisReplyContext(const std::string &command, Redis *me)
      : command(command), me(me) {
    DEBUG("new RedisReplyContext %X", this);
  }
  ~RedisReplyContext() { DEBUG("delete RedisReplyContext %X", this); }
};

void Redis::addWriteFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().addWriteInvoker(redis->_ac->c.fd, redis, [](void *pv) {
    redisAsyncHandleWrite(((Redis *)pv)->_ac);
  });
}

void Redis::addReadFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().addReadInvoker(redis->_ac->c.fd, redis, [](void *pv) {
    redisAsyncHandleRead(((Redis *)pv)->_ac);
  });
}

void Redis::delWriteFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().delWriteInvoker(redis->_ac->c.fd);
}

void Redis::delReadFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().delReadInvoker(redis->_ac->c.fd);
}

void Redis::cleanupFd(void *pv) {
  Redis *redis = (Redis *)pv;
  if (redis->_ac->c.fd < 0) {
    WARN(" cleanupFd for negative fd");
    return;
  }
  redis->thread().delAllInvoker(redis->_ac->c.fd);
}

Redis::Redis(Thread &thread, JsonObject config)
    : Actor(thread), _request(10, "request"), _response(100, "response") {
  _request.async(thread);
  _response.async(thread);
  _redisHost = config["host"] | "localhost";
  _redisPort = config["port"] | 6379;
  _reconnectOnConnectionLoss = config["reconnectOnConnectionLoss"] | true;

  _addReplyContext = config["addReplyContext"] | true;

  if (config["ignoreReplies"].is<JsonArray>()) {
    JsonArray ignoreReplies = config["ignoreReplies"].as<JsonArray>();
    for (JsonArray::iterator it = ignoreReplies.begin();
         it != ignoreReplies.end(); ++it) {
      _ignoreReplies.push_back(it->as<std::string>());
    }
  }
  _ac = 0;

  _jsonToRedis = new SinkFunction<Json>([&](const Json &docIn) {
    //    if (!_connected()) return; // otherwise first message lost
    std::string s;
    serializeJson(docIn, s);
    INFO("Redis:request  %s ", s.c_str());
    //  if (!docIn.is<JsonArray>()) return;
    Json *js = (Json *)&docIn;
    JsonArray array = js->as<JsonArray>();
    const char *argv[100];
    int argc;
    for (size_t i = 0; i < array.size(); i++) {
      if (!array[i].is<const char *>()) {
        WARN("Redis:request  %s not a string array", s.c_str());
        return;
      } else
        argv[i] = array[i].as<const char *>();
    }
    argc = array.size();
    int rc = redisAsyncCommandArgv(_ac, replyHandler,
                                   new RedisReplyContext(argv[0], this), argc,
                                   argv, NULL);
    if (rc)
      WARN("redisAsyncCommandArgv() failed %d : %s ", _ac->err, _ac->errstr);
  });
  _request >> _jsonToRedis;

  _command >> [&](std::string cmd) {
    int rc = redisAsyncCommand(_ac, replyHandler,
                               new RedisReplyContext(cmd.c_str(), this),
                               cmd.c_str(), NULL);

    if (rc) WARN("redisAsyncCommand() failed %d : %s ", _ac->err, _ac->errstr);
  };
};

Redis::~Redis() {
  INFO("~Redis()");
  _reconnectOnConnectionLoss = false;
  if (_connected()) disconnect();
  cleanupFd(this);
  delete _jsonToRedis;
}

void Redis::free_privdata(void *pvdata) {
  WARN(" freeing private data of context %X", pvdata);
}

void Redis::onPush(redisAsyncContext *ac, void *reply) {
  INFO(" PUSH received ");  // why do I never come here ????
}

int Redis::connect() {
  INFO("Connecting to Redis %s:%d ... ", _redisHost.c_str(), _redisPort);
  redisOptions options = {0};
  REDIS_OPTIONS_SET_TCP(&options, _redisHost.c_str(), _redisPort);
  options.connect_timeout = new timeval{3, 0};  // 3 sec
  options.async_push_cb = onPush;
  REDIS_OPTIONS_SET_PRIVDATA(&options, this, free_privdata);
  _ac = redisAsyncConnectWithOptions(&options);

  if (_ac == NULL || _ac->err) {
    WARN(" Connection %s:%d failed", _redisHost.c_str(), _redisPort);
    return ENOTCONN;
  }

  _ac->ev.addRead = addReadFd;
  _ac->ev.delRead = delReadFd;
  _ac->ev.addWrite = addWriteFd;
  _ac->ev.delWrite = delWriteFd;
  _ac->ev.cleanup = cleanupFd;
  _ac->ev.data = this;

  int rc = redisAsyncSetConnectCallback(
      _ac, [](const redisAsyncContext *ac, int status) {
        INFO("Redis connected status : %d fd : %d ", status, ac->c.fd);
        Redis *me = (Redis *)ac->c.privdata;
        me->_connected = true;
      });

  assert(rc == 0);

  rc = redisAsyncSetDisconnectCallback(
      _ac, [](const redisAsyncContext *ac, int status) {
        WARN("Redis disconnected status : %d fd : %d ", status, ac->c.fd)

        Redis *me = (Redis *)ac->c.privdata;
        me->_connected = false;
        if (me->_reconnectOnConnectionLoss) me->connect();
      });

  assert(rc == 0);

  redisAsyncSetPushCallback(_ac, onPush);

  return 0;
}

void Redis::stop() {
  _reconnectOnConnectionLoss = false;
  disconnect();
}

void Redis::disconnect() {
  INFO(" disconnect called");
  redisAsyncDisconnect(_ac);
}

void Redis::replyHandler(redisAsyncContext *ac, void *repl, void *pv) {
  redisReply *reply = (redisReply *)repl;
  Json doc;
  if (reply == 0) {
    WARN(" replyHandler caught null %d : %s ", ac->err, ac->errstr);
    return;  // disconnect ?
  };
  if ((reply->type == REDIS_REPLY_ARRAY || reply->type == REDIS_REPLY_PUSH) &&
      strcmp(reply->element[0]->str, "pmessage") == 0) {  // no context
    Redis *redis = (Redis *)ac->c.privdata;
    replyToJson(doc.as<JsonVariant>(), reply);
    std::string str;
    serializeJson(doc, str);
    INFO("Redis:push %s", str.c_str());
    redis->_response.on(doc);
    return;
  }

  RedisReplyContext *redisReplyContext = (RedisReplyContext *)pv;
  Redis *redis = redisReplyContext->me;
  std::string command = redisReplyContext->command;

  if (std::find(redis->_ignoreReplies.begin(), redis->_ignoreReplies.end(),
                command) != redis->_ignoreReplies.end()) {
    delete redisReplyContext;
    return;
  }
  if (redis->_addReplyContext && redisReplyContext->command != "psubscribe") {
    doc[0] = redisReplyContext->command;
    replyToJson(doc[1].to<JsonVariant>(), reply);
  } else {
    replyToJson(doc.as<JsonVariant>(), reply);
  }
  redis->_response.on(doc);

  std::string str;
  serializeJson(doc, str);
  INFO("Redis:reply '%s' =>  %s ", redisReplyContext->command.c_str(),
       str.c_str());
  delete redisReplyContext;
}

Sink<Json> &Redis::request() { return _request; }
Source<Json> &Redis::response() { return _response; }
Sink<std::string> &Redis::command() { return _command; }

void replyToJson(JsonVariant result, redisReply *reply) {
  if (reply == 0) {
    result.set(nullptr);
  };
  switch (reply->type) {
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_BIGNUM:
    case REDIS_REPLY_VERB:
    case REDIS_REPLY_STRING:
      result.set(reply->str);
      break;

    case REDIS_REPLY_DOUBLE:
      result.set(reply->dval);
      break;

    case REDIS_REPLY_INTEGER:
      result.set(reply->integer);
      break;

    case REDIS_REPLY_NIL:
      result.set(nullptr);
      break;

    case REDIS_REPLY_BOOL:
      result.set(reply->integer != 0);
      break;

    case REDIS_REPLY_MAP:
      for (size_t i = 0; i < reply->elements; i += 2)
        replyToJson(result[reply->element[i]->str].to<JsonVariant>(),
                    reply->element[i + 1]);
      break;

    case REDIS_REPLY_SET:
    case REDIS_REPLY_PUSH:
    case REDIS_REPLY_ARRAY:
      for (size_t i = 0; i < reply->elements; i++)
        replyToJson(result.addElement(), reply->element[i]);
      break;
    default: {
      result.set(" Unhandled reply to JSON type");
      break;
    }
  }
}

void Redis::publish(std::string channel, std::string message) {
  Json doc;
  JsonArray array = doc.to<JsonArray>();
  array.add("publish");
  array.add(channel);
  array.add(message);
  _request.on(doc);
}

DynamicJsonDocument replyToJson(redisReply *reply) {
  DynamicJsonDocument doc(10240);
  replyToJson(doc.as<JsonVariant>(), reply);
  doc.shrinkToFit();
  return doc;
}