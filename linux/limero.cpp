#include "limero.h"

#include <asm-generic/ioctls.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <algorithm>
#include <thread>

/*
 _____ _                        _
|_   _| |__  _ __ ___  __ _  __| |
  | | | '_ \| '__/ _ \/ _` |/ _` |
  | | | | | | | |  __/ (_| | (_| |
  |_| |_| |_|_|  \___|\__,_|\__,_|
*/

Thread::Thread(const char *__name) : Named(__name)
{
  _readPipe = -1;
  _writePipe = -1;
  _maxFd = 0;
  createQueue();
}

void Thread::buildFdSet()
{
  FD_ZERO(&_rfds);
  FD_ZERO(&_wfds);
  FD_ZERO(&_efds);
  _maxFd = _readPipe;
  FD_SET(_readPipe, &_rfds);
  FD_SET(_readPipe, &_efds);
  for (const auto &myPair : _readInvokers)
  {
    int fd = myPair.first;
    FD_SET(fd, &_rfds);
    if (fd > _maxFd)
      _maxFd = fd;
  }
  for (const auto &myPair : _errorInvokers)
  {
    int fd = myPair.first;
    FD_SET(fd, &_efds);
    if (fd > _maxFd)
      _maxFd = fd;
  }
  for (const auto &myPair : _writeInvokers)
  {
    int fd = myPair.first;
    FD_SET(fd, &_wfds);
    if (fd > _maxFd)
      _maxFd = fd;
  }
  _maxFd += 1;
}

void Thread::addReadInvoker(int fd, void *arg, CallbackFunction fn)
{
  Callback cb = {fn, arg};
  _readInvokers.emplace(fd, cb);
  //  _readInvokers[fd] = {fn, arg};
  buildFdSet();
}

void Thread::delReadInvoker(const int fd)
{
  _readInvokers.erase(fd);
  buildFdSet();
}

void Thread::addWriteInvoker(int fd, void *arg, CallbackFunction fn)
{
  Callback cb = {fn, arg};
  _writeInvokers.emplace(fd, cb);
  buildFdSet();
}

void Thread::delWriteInvoker(const int fd)
{
  _writeInvokers.erase(fd);
  buildFdSet();
}

void Thread::addErrorInvoker(int fd, void *arg, CallbackFunction fn)
{
  Callback cb = {fn, arg};
  _errorInvokers.emplace(fd, cb);
  buildFdSet();
}

void Thread::delErrorInvoker(const int fd)
{
  _errorInvokers.erase(fd);
  buildFdSet();
}

void Thread::delAllInvoker(int fd)
{
  _readInvokers.erase(fd);
  _errorInvokers.erase(fd);
  _writeInvokers.erase(fd);
  buildFdSet();
}

int Thread::waitInvoker(uint32_t timeout)
{
  fd_set rfds = _rfds;
  fd_set wfds = _wfds;
  fd_set efds = _efds;
  Invoker *invoker;

  struct timeval tv;
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout * 1000) % 1000000;

  int rc = select(_maxFd, &rfds, &wfds, &efds, &tv);

  if (rc < 0)
  {
    WARN(" select() : error : %s (%d)", strerror(errno), errno);
    return rc;
  }
  else if (rc > 0)
  { // one of the fd was set
    for (auto &myPair : _writeInvokers)
    {
      if (myPair.first > 100)
        WARN(" fd out of range ");
    }
    if (FD_ISSET(_readPipe, &rfds))
    {
      ::read(_readPipe, &invoker, sizeof(Invoker *)); // read 1 event handler
      invoker->invoke();
    }

    for (auto &myPair : _readInvokers)
    {
      if (FD_ISSET(myPair.first, &rfds))
      {
        myPair.second.fn(myPair.second.arg);
        break;
      }
    }
    for (auto &myPair : _writeInvokers)
    {
      if (FD_ISSET(myPair.first, &wfds))
      {
        myPair.second.fn(myPair.second.arg); // can impact _writeInvokers
        break;
      }
    }
    for (auto &myPair : _errorInvokers)
    {
      if (FD_ISSET(myPair.first, &efds))
      {
        myPair.second.fn(myPair.second.arg);
        break;
      }
    }
    if (FD_ISSET(_readPipe, &efds))
    {
      WARN("pipe  error : %s (%d)", strerror(errno), errno);
      return ECOMM;
    }
    return 0;
  }
  else
  {
    DEBUG(" timeout %lu", timeout );
    return ETIMEDOUT;
  }
  return ECOMM;
}

void Thread::createQueue()
{
  int rc = pipe(_pipeFd);
  _writePipe = _pipeFd[1];
  _readPipe = _pipeFd[0];
  if (rc < 0)
    WARN("Queue creation failed %d %s ", errno, strerror(errno));
  if (fcntl(_writePipe, F_SETFL, O_NONBLOCK) < 0)
  {
    WARN("Failed to set pipe blocking mode: %s (%d)", strerror(errno), errno);
  }
}
TimerSource &Thread::createTimer(uint32_t interval, bool repeat, bool active, const char *__name)
{
  TimerSource *timer = new TimerSource(interval, repeat, active, __name);
  _timers.push_back(timer);
  return *timer;
}

void Thread::deleteTimer(TimerSource &ts)
{
  auto pos = std::find(_timers.begin(), _timers.end(), &ts);
  if (pos != _timers.end())
    _timers.erase(pos);
}

void SetThreadName(std::thread *thread, const char *threadName)
{
  auto handle = thread->native_handle();
  pthread_setname_np(handle, threadName);
}

void Thread::start()
{
  INFO("Thread %s started", name());
  _thread = new std::thread(&Thread::run, this);

  SetThreadName(_thread, name());
}

int Thread::queue(Invoker *invoker)
{
  //  INFO("Thread '%s' >>> '%lX", _name.c_str(), invoker);
  if (_writePipe)
    if (write(_writePipe, (const char *)&invoker, sizeof(Invoker *)) == -1)
    {
      WARN("Thread '%s' queue overflow [%X] : [%d] %s ", name(), invoker, errno, strerror(errno));
      return ENOBUFS;
    }
  return 0;
};
/*
void Thread::wake() {
  if (_writePipe) {
    Invoker *invoker = new Invoker();
    if (write(_writePipe, (const char *)&invoker, sizeof(Invoker *)) == -1) {
      WARN("Thread '%s' queue overflow [%X]", name(), invoker);
    }
  }
}
*/
void Thread::run()
/*
{
  while (true)
  {
    uint32_t min_wait = 1; // cycle fast on lock
    if (try_lock())
    {
      // read all invokers
      Invoker *invoker;
      while (_invokers.pop(invoker))
      {
        invoker->invoke();
      }

      // check all timers
      for (TimerSource *timer : _timers)
      {
        if (timer->isExpired())
          timer->invoke();
      }
      min_wait = minWait();
    }
    wait(minWait());
  }
} */
{
  INFO("Thread '%s' started ", name());
  buildFdSet();
  while (true)
  {
    for (TimerSource *timer : _timers)
    {
      if (timer->isExpired())
        timer->invoke();
    }
    int32_t waitTime = minWait();
    int rc = waitInvoker(waitTime);
  }
}

uint32_t Thread::minWait()
{

  uint32_t waitTime = 10000;
  uint64_t now = Sys::millis();
  for (TimerSource *timer : _timers)
  {
    if (timer->expiresAt() < now)
      return 0;
    uint32_t delta = timer->expiresAt() - now;
    if (delta < waitTime)
      waitTime = delta;
  }
  return waitTime;
}

bool Thread::inThread()
{
  return _thread->get_id() == std::this_thread::get_id();
}
