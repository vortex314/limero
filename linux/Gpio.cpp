#include <Gpio.h>
#ifdef __arm__
#define HAS_GPIO 1
#endif
#ifdef HAS_GPIO
#include <wiringPi.h>
#else
#define INPUT "INPUT"
#define OUTPUT "OUTPUT"
#define HIGH "HIGH"
#define LOW "LOW"
#define pinMode(x, y) INFO("  gpio mode pin %d = %s", x, y)
#define digitalWrite(__pin, __value) \
  INFO("gpio value pin %d = %s ", __pin, __value)
#endif

void Gpio::init()
{
#ifdef HAS_GPIO
  wiringPiSetupGpio();
#endif
  INFO(" this GPIO doesn't do anything !");
}

Gpio::Gpio(int pin)
{
  _pin = pin;
  _mode = M_INPUT;

  mode >> [&](const std::string &m) {
    _mode = m[0] == 'O' ? M_OUTPUT : M_INPUT;
    INFO(" setting pin %d mode to %s ", _pin,
         _mode == M_INPUT ? "INPUT" : "OUTPUT");
#ifdef HAS_GPIO
    if (_mode == M_INPUT)
      pinMode(_pin, INPUT);
    else
      pinMode(_pin, OUTPUT);
    mode = _mode == M_INPUT ? "INPUT" : "OUTPUT";
#else
    INFO(" GPIO %d ", pin);
#endif
  };

  value >> [&](const int &v) {
    _value = v ? 1 : 0;
    INFO(" setting pin %d value to %d ", _pin, _value);
#ifdef HAS_GPIO
    if (_mode == M_OUTPUT)
      digitalWrite(_pin, _value ? HIGH : LOW);
#endif
    value = _value;
  };
}

Gpio::~Gpio() {}
