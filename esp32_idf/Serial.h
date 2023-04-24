#ifndef SERIAL_H
#define SERIAL_H
#undef min
#undef max
#include <Hardware.h>
#include <Serial.h>
#include <limero.h>

#include "driver/uart.h"

class Serial : public Actor
{
  UART &_uart;
  QueueFlow<Bytes> _txd;
  ValueFlow<Bytes> _rxd;
  Bytes  _rxdBytes;

public:
  static void onRxd(void *);
  Serial(Thread &thr,UART& uart);
  ~Serial();
  void init();
  Source<Bytes>& rxd();
  Sink<Bytes>& txd();

};

#endif // MQTTSERIAL_H
