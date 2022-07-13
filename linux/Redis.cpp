#include <Redis.h>
#include <assert.h>

#include <algorithm>

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

void Redis::responseFailure(int rc, std::string message) {
  Json response;
  response[0] = "error";
  response[2] = message;
  response[1] = rc;
  _response.on(response);
}

Redis::Redis(Thread &thread, JsonObject config)
    : Actor(thread), _request(10, "request"), _response(100, "response") {
  _request.async(thread);
  _response.async(thread);
  _redisHost = config["host"] | "localhost";
  _redisPort = config["port"] | 6379;
  _connectionStatus = CS_DISCONNECTED;
  std::string str;
  serializeJson(config, str);
  INFO("Redis host: %s config: %s", _redisHost.c_str(), str.c_str());
  _reconnectOnConnectionLoss = config["reconnectOnConnectionLoss"] | true;

  _addReplyContext = config["addReplyContext"] | true;

  _response >> [](const Json &response) {
    std::string str;
    serializeJson(response, str);
    INFO("Redis response: '%s'", str.c_str());
  };

  _request >> [](const Json &request) {
    std::string str;
    serializeJson(request, str);
    INFO("Redis request: '%s'", str.c_str());
  };

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
    if (!_connected() && _connectionStatus == CS_DISCONNECTED) {
      responseFailure(ENOTCONN, "Not Connected");
      return;
    }

    Json *js = (Json *)&docIn;
    if (!js->is<JsonArray>()) {
      responseFailure(EINVAL, "Not a string array.");
      return;
    }
    JsonArray jsonRequest = js->as<JsonArray>();
    const char *argv[100];
    int argc;
    for (size_t i = 0; i < jsonRequest.size(); i++) {
      if (!jsonRequest[i].is<const char *>()) {
        WARN("Redis:request not a string jsonRequest");
        responseFailure(EINVAL, "not a string array ");
        return;
      } else
        argv[i] = jsonRequest[i].as<const char *>();
    }
    argc = jsonRequest.size();
    int rc = redisAsyncCommandArgv(_ac, replyHandler,
                                   new RedisReplyContext(argv[0], this), argc,
                                   argv, NULL);
    if (rc) {
      WARN("redisAsyncCommandArgv() failed %d ", _ac->err);
      responseFailure(rc, "redisAsyncCommandArgv() failed ");
    }
  });
  _request >> _jsonToRedis;

  _command >> [&](std::string cmd) {
    if (!_connected()) {
      responseFailure(ENOTCONN, "Not Connected");
      return;
    }
    int rc = redisAsyncCommand(_ac, replyHandler,
                               new RedisReplyContext(cmd.c_str(), this),
                               cmd.c_str(), NULL);

    if (rc) {
      WARN("redisAsyncCommand() failed %d : %s ", _ac->err, _ac->errstr);
      responseFailure(rc, "redisAsyncCommand() failed ");
    }
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
  replyHandler(ac, reply, NULL);
}

int Redis::connect() {
  INFO("Connecting to Redis %s:%d ... ", _redisHost.c_str(), _redisPort);
  redisOptions options = {0};
  REDIS_OPTIONS_SET_TCP(&options, _redisHost.c_str(), _redisPort);
  options.connect_timeout = new timeval{3, 0};  // 3 sec
  options.async_push_cb = onPush;
  REDIS_OPTIONS_SET_PRIVDATA(&options, this, free_privdata);
  _connectionStatus = CS_CONNECTING;
  _ac = redisAsyncConnectWithOptions(&options);

  if (_ac == NULL || _ac->err) {
    WARN(" Connection %s:%d failed", _redisHost.c_str(), _redisPort);
    _connectionStatus = CS_DISCONNECTED;
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
        me->_connectionStatus = CS_CONNECTED;
      });

  assert(rc == 0);

  rc = redisAsyncSetDisconnectCallback(
      _ac, [](const redisAsyncContext *ac, int status) {
        WARN("Redis disconnected status : %d fd : %d ", status, ac->c.fd)

        Redis *me = (Redis *)ac->c.privdata;
        me->_connected = false;
        me->_connectionStatus = CS_DISCONNECTED;
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

void Redis::makeEnvelope(JsonVariant envelope,
                         RedisReplyContext *redisReplyContext) {
  std::string command = redisReplyContext->command;
  INFO("Redis context command: '%s'", command.c_str());

  if (std::find(_ignoreReplies.begin(), _ignoreReplies.end(), command) !=
      _ignoreReplies.end()) {
    delete redisReplyContext;
    return;
  }
  if (_addReplyContext) {
    envelope[0] = redisReplyContext->command;
  }
}

bool isPsubscribe(JsonVariant v) {
  if (v.is<JsonArray>() && v[0].is<std::string>() && v[0] == "psubscribe")
    return true;
  return false;
}

bool isPmessage(JsonVariant v) {
  if (v.is<JsonArray>() && v[0].is<std::string>() && v[0] == "pmessage")
    return true;
  return false;
}

void Redis::replyHandler(redisAsyncContext *ac, void *repl, void *pv) {
  redisReply *reply = (redisReply *)repl;
  Json replyInJson;
  Json envelope;
  RedisReplyContext *redisReplyContext =
      (RedisReplyContext *)pv;  // can be null
  Redis *redis = (Redis *)ac->c.privdata;

  if (reply == 0) {
    WARN(" replyHandler caught null %d  ", ac->err);
    //    redis->responseFailure(EINVAL, "replyHandler caught null ");
    return;  // disconnect ?
  };
  redisReplyToJson(replyInJson.as<JsonVariant>(), reply);
  if (redis->_addReplyContext && redisReplyContext &&
      !isPsubscribe(replyInJson) && !isPmessage(replyInJson)) {
    envelope[0] = redisReplyContext->command;
    envelope[1] = replyInJson;
    redis->_response.on(envelope);
    delete redisReplyContext;
  } else {
    redis->_response.on(replyInJson);
  }
  return;
}

Sink<Json> &Redis::request() { return _request; }
Source<Json> &Redis::response() { return _response; }
Sink<std::string> &Redis::command() { return _command; }

void redisReplyToJson(JsonVariant result, redisReply *reply) {
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
        redisReplyToJson(result[reply->element[i]->str].to<JsonVariant>(),
                         reply->element[i + 1]);
      break;

    case REDIS_REPLY_SET:
    case REDIS_REPLY_PUSH:
    case REDIS_REPLY_ARRAY:
      for (size_t i = 0; i < reply->elements; i++)
        redisReplyToJson(result.addElement(), reply->element[i]);
      break;
    default: {
      result.set(" Unhandled reply to JSON type");
      break;
    }
  }
}

void Redis::publish(std::string channel, std::string message) {
  Json doc;
  JsonArray jsonRequest = doc.to<JsonArray>();
  jsonRequest.add("publish");
  jsonRequest.add(channel);
  jsonRequest.add(message);
  _request.on(doc);
}

DynamicJsonDocument replyToJson(redisReply *reply) {
  uint32_t size = 10240;
  while (size < 10000000) {
    DynamicJsonDocument doc(size);
    redisReplyToJson(doc, reply);
    if (!doc.overflowed()) {
      doc.shrinkToFit();
      return doc;
    }
    size *= 2;
    INFO(" doubling JSON size %d", size);
  }
  DynamicJsonDocument doc(10240);
  doc["error"] = "JSON overflow error > 10M bytes";
  doc["errno"] = ENOMEM;
  return doc;
}