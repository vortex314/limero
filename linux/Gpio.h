#ifndef _GPIO_H_
#define _GPIO_H_
#include <limero.h>
#include <wiringPi.h>



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

extern Gpio* gpio[];
typedef void (*Handler)(void);

template < int P> 
void GpioIsr(void) {
  gpio[P]->value = digitalRead(gpio[P]->_pin);};
#endif
