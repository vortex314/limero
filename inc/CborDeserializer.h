#ifndef AA72A63D_3140_4FF2_BCAF_6F744DBBD62B
#define AA72A63D_3140_4FF2_BCAF_6F744DBBD62B
#include <assert.h>
#include <cbor.h>

#include <string>
#include <vector>

typedef std::vector<uint8_t> Bytes;

class CborDeserializer {
  CborParser _decoder;
  std::vector<CborValue *> _stack;
  CborValue *_currentValue;
  CborError _cborError;
  uint8_t *_buffer;
  size_t _size;
  size_t _capacity;
  int _error;

 public:
  CborDeserializer(size_t size);
  ~CborDeserializer();
  CborDeserializer &begin();
  CborDeserializer &end();
  template <typename T>
  CborDeserializer &operator>>(T &t) {
    get(t);
    return *this;
  };
  CborDeserializer &startArray();
  CborDeserializer &endArray();
  CborDeserializer &startMap();
  CborDeserializer &endMap();
  CborDeserializer &get(Bytes &t);
  CborDeserializer &get(std::string &t);
  CborDeserializer &get(int &t);
  //  CborDeserializer &get(int32_t &t, const char *n="",const char
  //  *d="");
  CborDeserializer &get(uint32_t &t);
  CborDeserializer &get(int64_t &t);
  CborDeserializer &get(uint64_t &t);
  CborDeserializer &get(float &t);
  CborDeserializer &get(double &t);
  CborDeserializer &get(const int &t);
  CborDeserializer &fromBytes(const Bytes &bs);
  inline bool ok() { return _error == 0; };
};
#endif /* AA72A63D_3140_4FF2_BCAF_6F744DBBD62B */
