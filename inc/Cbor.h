#include <stdint.h>
#include <cstring>
#include <errno.h>
#include <cstdint>
#include <vector>
#include <string>

//=========================================================================================
// ================== CBOR ENCODER STREAM ===================

//==============================================================

#define MAJOR_TYPE(x) ((x) >> 5)

typedef enum CborType
{
    CborIntegerType = 0x00,
    CborByteStringType = 0x40,
    CborTextStringType = 0x60,
    CborArrayType = 0x80,
    CborMapType = 0xa0,
    CborTagType = 0xc0,
    CborSimpleType = 0xe0,
    CborBooleanType = 0xf5,
    CborBooleanTrue = 0xf5,
    CborBooleanFalse = 0xf4,
    CborNullType = 0xf6,
    CborUndefinedType = 0xf7,
    CborHalfFloatType = 0xf9,
    CborFloatType = 0xfa,
    CborDoubleType = 0xfb,
    CborBreakType = 0xff,
} CborType;
class Special
{
public:
    int _x;
    Special(int x) : _x(x) {}
};

extern Special Start;
extern Special End;
extern Special Break;

typedef std::vector<uint8_t> Bytes;
class CborEncoder : public Bytes
{
public:
    CborEncoder(uint32_t);

    CborEncoder &start();
    CborEncoder &end();
//    CborEncoder &write(int);
 //   CborEncoder &write(unsigned int);
    CborEncoder &write(int32_t);
    CborEncoder &write(uint32_t);
    CborEncoder &write(int64_t);
    CborEncoder &write(uint64_t);
    CborEncoder &write(float);
    CborEncoder &write(double);
    CborEncoder &write(const char *);
    CborEncoder &write(bool);
    CborEncoder &write(std::vector<uint8_t> &);
    CborEncoder &write(const std::string &);
    CborEncoder &writeArrayStart();
    CborEncoder &writeArrayEnd();
    CborEncoder &writeArray(uint64_t);
    CborEncoder &writeMapStart();
    CborEncoder &writeMapEnd();
    CborEncoder &writeMap(uint64_t);
    CborEncoder &writeNull();
    CborEncoder &writeUndefined();
    CborEncoder &writeBreak();
    CborEncoder &write(char);
    CborEncoder &writeTag(uint64_t);
    template <typename T>
    CborEncoder &operator<<(T v)
    {
        return write(v);
    }
    CborEncoder &operator<<(Special v)
    {
        if (v._x == 1)
            return start();
        if (v._x == 2)
            return end();
        if (v._x == 3)
            return writeBreak();
        return *this;
    }

    //   inline uint8_t *buffer() { return data(); }
    inline void error(int x)
    {
        _error = x;
    }
    inline bool ok() { return _error == 0; }

private:
    uint32_t _capacity;
    int _error;
    void addByte(uint8_t value);
    void write_type_and_value(uint8_t, uint64_t);
};

//====================================================================================
class CborHeader
{
public:
    bool is_null() const { return firstByte == CborNullType; }
    bool is_undefined() const { return firstByte == CborUndefinedType; }
    bool is_bool() const { return firstByte == CborBooleanFalse || firstByte == CborBooleanTrue; }
    bool is_break() const { return firstByte == CborBreakType; }
    bool is_bytes() const { return MAJOR_TYPE(firstByte) == 2; }
    bool is_string() const { return MAJOR_TYPE(firstByte) == 3; }
    bool is_array() const { return MAJOR_TYPE(firstByte) == MAJOR_TYPE(CborArrayType); }
    bool is_indefinite_array() const { return firstByte == 0x9f; }
    bool is_map() const { return MAJOR_TYPE(firstByte) == 5; }
    bool is_indefinite_map() const { return firstByte == 0xb6; }
    bool is_tag() const { return MAJOR_TYPE(firstByte) == MAJOR_TYPE(CborTagType); }
    bool is_float() const { return firstByte == CborFloatType; }
    bool is_double() const { return firstByte == CborDoubleType; }
    bool is_uint() const { return MAJOR_TYPE(firstByte) == 0; }
    bool is_int() const { return MAJOR_TYPE(firstByte) == 1; }
    bool as_bool() const
    {
        if (firstByte == 0xf4)
            return false;
        return true;
    }

    uint64_t as_uint() const
    {
        return val;
    }

    int64_t as_int() const
    {
        if (MAJOR_TYPE(firstByte) == 0)
            return val;
        if (MAJOR_TYPE(firstByte) != 1)
            return -1 - val;
    }

    uint64_t as_bytes_header() const
    {
        return val;
    }

    uint64_t as_string_header() const
    {
        return val;
    }

    uint64_t as_array() const
    {
        return val;
    }

    uint64_t as_map() const
    {
        return val;
    }

    uint64_t as_tag() const
    {
        return val;
    }

    uint8_t firstByte = 0;
    uint64_t val = 0;
};

class CborDecoder : public Bytes
{
public:
    CborDecoder(uint32_t);
    void reset();
    bool checkCrc();
    void addUnEscaped(uint8_t);
    void feed(const Bytes&);
    void addUnEscaped(const Bytes& );
    uint8_t *buffer() { return data(); }
    //   uint32_t size() { return _writePtr; }
    CborDecoder &rewind();
    uint8_t get_byte();
    void put_byte(uint8_t);
    void put_bytes(const uint8_t *, uint32_t);
    void put_bytes(const Bytes &);

    CborDecoder &readArrayStart();
    CborDecoder &readArrayEnd();
    CborDecoder &readMapStart();
    CborDecoder &readMapEnd();
    CborDecoder &read(bool &);
    CborDecoder &read(int &);
    CborDecoder &read(unsigned int &);
    CborDecoder &read(long &);
    CborDecoder &read(unsigned long &);
    CborDecoder &read(long long &);
    CborDecoder &read(unsigned long long &);
    CborDecoder &read(float &);
    CborDecoder &read(double &);
    CborDecoder &read(char *, uint32_t);
    CborDecoder &read(std::string &);
    CborDecoder &read(const char *);
    CborDecoder &read(const char);
    CborHeader peek();
    inline void error(int x)
    {
        _error = x;
    }
    inline bool ok() { return _error == 0; }
    template <typename T>
    CborDecoder &operator>>(T &v)
    {
        return read(v);
    }

private:
    uint32_t _capacity;
    uint32_t _readPtr;
    int _error;
    CborHeader _h;
    bool next();
};
