#ifndef __PPP_H__
#define __PPP_H__
#include <limero.h>
#include <stdint.h>
#include <unistd.h>

#include <vector>

typedef std::vector<uint8_t> Bytes;

#define PPP_FCS_SIZE 2

#define PPP_MASK_CHAR 0x20
#define PPP_ESC_CHAR 0x7D
#define PPP_FLAG_CHAR 0x7E

class PppDeframer : public Flow<Bytes, Bytes>,public Actor
{
  Bytes _buffer;
  size_t _maxFrameLength;
  bool _escFlag;

public:
  Source<Bytes> garbage;
  PppDeframer(Thread&,size_t maxFrameLength);
  bool checkCrc(Bytes &bs);
  void on(const Bytes &in);
};

class PPP : public Actor {
  uint64_t _lastFrameFlag;
  size_t _maxFrameLength;
  Bytes _buffer;

  Flow<Bytes, Bytes> _frame;
  PppDeframer *_deframe;
  Source<Bytes> _garbage;

 public:
  PPP(Thread&,size_t);
  ~PPP();

  void addEscaped(Bytes &out, uint8_t c);
  bool handleFrame(const Bytes &bs);
  void handleRxd(const Bytes &bs);
  void request();

  Flow<Bytes, Bytes> &frame();
  Flow<Bytes, Bytes> &deframe();
  Source<Bytes> & garbage() { return _deframe->garbage;};
};

#endif
