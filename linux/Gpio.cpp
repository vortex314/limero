#include <Gpio.h>
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

Gpio *gpio[40];

#define HANDLER(_X_)                     \
  void handler##_X_(void)                \
  {                                      \
    INFO(" pin interrupt : %d ", _X_);   \
    gpio[_X_]->value = digitalRead(_X_); \
  }

static uint8_t gpioRaspberry[] = {4, 9, 10, 17, 18, 22, 23, 24, 25, 27};
HANDLER(4);
HANDLER(9);
HANDLER(10);
HANDLER(17);
HANDLER(18);
HANDLER(22);
HANDLER(23);
HANDLER(24);
HANDLER(25);
HANDLER(27);

Handler handler[] = {(Handler)0, (Handler)1, (Handler)2, (Handler)3, handler4, (Handler)5, (Handler)6, (Handler)7,
                     (Handler)8, handler9, handler10,
                     (Handler)11, (Handler)12, (Handler)13, (Handler)14, (Handler)15, handler17, handler18,
                     (Handler)19, (Handler)20, (Handler)21, handler22, handler23, handler24, handler25,
                     (Handler)26, handler27};

void Gpio::init()
{
#ifdef HAS_GPIO
  wiringPiSetupGpio();
#else
  INFO(" this GPIO doesn't do anything !");
#endif
}

Gpio::Gpio(Thread &thread, int pin) : _pollTimer(thread, 1000, true, "gpioPollTimer")
{
  _pin = pin;
  _mode = M_INPUT;
  gpio[_pin] = this;

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
    INFO(" stub GPIO %d mode %s ", pin, _mode.c_str());
#endif
  };

  value >> [&](const int &v) {
    _value = v ? 1 : 0;
    INFO(" setting pin %d value to %d ", _pin, _value);
#ifdef HAS_GPIO
    if (_mode == M_OUTPUT)
      digitalWrite(_pin, _value ? HIGH : LOW);
#else
    INFO(" stub GPIO %d write %d ", pin, _value);
#endif
    value = _value;
  };
  _pollTimer >> [&](const TimerMsg &tm) {
#ifdef HAS_GPIO
    if (_mode == M_INPUT)
      value = digitalRead(_pin);
    else
      value = _value;
    INFO(" GPIO %d read %d ", _pin, value());
#else
    INFO(" stub GPIO %d read %d ", pin, _value);
#endif
  };
#ifdef HAS_GPIO
  if ((int)handler[_pin] > 100)
    wiringPiISR(_pin, INT_EDGE_BOTH, handler[_pin]);
#endif
}

Gpio::~Gpio() {}
