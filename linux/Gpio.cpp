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
#ifdef HAS_GPIO

#define HANDLER(_X_)                                     \
  void handler##_X_(void)                                \
  {                                                      \
    INFO(" pin interrupt : %d : 0x%X ", _X_, gpio[_X_]); \
    gpio[_X_]->_value = digitalRead(_X_);                \
  }

//    for (int i = 0; i < 30; i++)                         \
//      INFO(" gpio[%d]=%X", i, gpio[i]);                  \

std::vector<int> Gpio::raspberryGpio{0, 1, 2, 3, 4, 5, 7, 21, 22, 23, 24, 25, 26, 27, 28, 29}; // removed 6
HANDLER(0);
HANDLER(1);
HANDLER(2);
HANDLER(3);
HANDLER(4);
HANDLER(5);
HANDLER(6);
HANDLER(7);
HANDLER(8);
HANDLER(9);
HANDLER(10);
HANDLER(11);
HANDLER(12);
HANDLER(13);
HANDLER(14);
HANDLER(15);
HANDLER(16);
HANDLER(17);
HANDLER(18);
HANDLER(19);
HANDLER(20);
HANDLER(21);
HANDLER(22);
HANDLER(23);
HANDLER(24);
HANDLER(25);
HANDLER(26);
HANDLER(27);

Handler handler[] = {handler0, handler1, handler2, handler3, handler4, handler5,
                     handler6, handler7, handler8, handler9, handler10,
                     handler11, handler12, handler13, handler14, handler15,
                     handler16, handler17, handler18, handler19, handler20,
                     handler21, handler22, handler23, handler24, handler25,
                     handler26, handler27};
#endif

void Gpio::init()
{
#ifdef HAS_GPIO
  wiringPiSetup();
#else
  INFO(" this GPIO doesn't do anything, completely stubbed  !");
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
    _mode == M_INPUT ? "INPUT" : "OUTPUT";
#else
    INFO(" stub GPIO %d mode %s ", _pin, mode().c_str());
#endif
  };

  value >> [&](const int &v) {
    _value = v ? 1 : 0;
    INFO(" setting pin %d value to %d ", _pin, _value);
#ifdef HAS_GPIO
    if (_mode == M_OUTPUT)
      digitalWrite(_pin, _value ? HIGH : LOW);
#else
    INFO(" stub GPIO %d write %d ", _pin, _value);
#endif
  };
  _pollTimer >> [&](const TimerMsg &tm) {
#ifdef HAS_GPIO
    _value = digitalRead(_pin);
    if (value() != _value)
      value = _value;
#else
    INFO(" stub GPIO %d read %d ", _pin, _value);
#endif
  };
#ifdef HAS_GPIO_ISR // de-activated interrupts, crashes mqtt ?
  if ((int)handler[_pin] > 100)
    wiringPiISR(_pin, INT_EDGE_BOTH, handler[_pin]);
#endif
}

Gpio::~Gpio() {}
