#ifndef _GPIO_H_
#define _GPIO_H_
#include <limero.h>
#ifdef HAS_GPIO
#include <wiringPi.h>
#endif



class Gpio {
  int _pin;

 public:
  enum Mode { M_INPUT, M_OUTPUT };
  Gpio(Thread& ,int);
  ~Gpio();
  ValueFlow<std::string> mode;
  ValueFlow<int> value;
  static void init();
  void request();

 private:
  Mode _mode;
  int _value;
  TimerSource _pollTimer;
};

typedef void (*Handler)(void);

#endif
