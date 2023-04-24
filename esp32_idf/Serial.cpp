#include <Serial.h>

#include <StringUtility.h>

Serial::Serial(Thread &thr,UART& uart)
    : Actor(thr),
      _uart(uart),_txd(thr, 10,"Serial:txd"),_rxd(thr,"Serial:rxd")
{
_rxdBytes.reserve(1024);
_txd >> [&](const Bytes &b) {
    _uart.write(b.data(), b.size());
  };
}
Serial::~Serial() {}

void Serial::init()
{
  _uart.setClock(115200);
  _uart.onRxd(onRxd, this);
  _uart.mode("8N1");
  _uart.init();
}

void Serial::onRxd(void *me)
{
  Serial *serial = (Serial *)me;
  serial->_rxdBytes.clear();
  static Bytes bytes;
  while (serial->_uart.hasData())
  {
    bytes.clear();
    serial->_uart.read(bytes);
    for (int i = 0; i < bytes.size(); i++)
      serial->_rxdBytes.push_back(bytes[i]);
  }
  if (serial->_rxdBytes.size() > 0)
  {
    serial->_rxd.on(serial->_rxdBytes);
  }
}


Source<Bytes>& Serial::rxd()
{
  return _rxd;
}
Sink<Bytes>& Serial::txd()
{
  return _txd;
}


