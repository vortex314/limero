#ifndef _GPIO_H_
#define _GPIO_H_
#include <limero.h>
class Gpio {
  int _pin;

 public:
  enum Mode { M_INPUT, M_OUTPUT };
  Gpio(int);
  ~Gpio();
  ValueFlow<std::string> mode;
  ValueFlow<int> value;
  static void init();

 private:
  Mode _mode;
  int _value;
};
#endif
