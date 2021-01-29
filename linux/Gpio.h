#ifndef _GPIO_H_
#define _GPIO_H_
#include <limero.h>
#include <vector>
#ifdef HAS_GPIO
#include <wiringPi.h>
#endif

class Gpio
{
  int _pin;

public:
  static std::vector<int> raspberryGpio;
  enum Mode
  {
    M_INPUT,
    M_OUTPUT
  };
  Gpio(Thread &, int);
  ~Gpio();
  ValueFlow<std::string> mode;
  ValueFlow<int> value;
  static void init();
  void request();
  int _value;

private:
  Mode _mode;
  TimerSource _pollTimer;
};

typedef void (*Handler)(void);

#endif
