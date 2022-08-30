#include <stdint.h>
#include <cstring>
#include <errno.h>
#include <cstdint>
#include <vector>
#include <string>

//=========================================================================================
// ================== CBOR ENCODER STREAM ===================

//==============================================================

class Fcs
{
    uint16_t _fcs;

public:
    Fcs() { _fcs = 0xFFFF; }
    bool hasSpace(int size = 1) { return true; };
    bool write(uint8_t b);
    uint16_t intermediate() const { return _fcs; };
    uint16_t result() const { return ~_fcs; }
    void clear() { _fcs = 0xFFFF; }
};

#define PPP_FCS_SIZE 2

#define PPP_MASK_CHAR 0x20
#define PPP_ESC_CHAR 0x7D
#define PPP_FLAG_CHAR 0x7E

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
    CborNullType = 0xf6,
    CborUndefinedType = 0xf7,
    CborHalfFloatType = 0xf9,
    CborFloatType = 0xfa,
    CborDoubleType = 0xfb,
    CborBreakType = 0xff,
    CborInvalidType = 0xff /* equivalent to the break byte, so it will never be used */
} CborType;

class ProtocolEncoder
{
public:
    ProtocolEncoder(uint32_t);

    ProtocolEncoder &start();
    ProtocolEncoder &end();
    ProtocolEncoder &write(int);
    ProtocolEncoder &write(unsigned int);
    ProtocolEncoder &write(int32_t);
    ProtocolEncoder &write(uint32_t);
    ProtocolEncoder &write(int64_t);
    ProtocolEncoder &write(uint64_t);
    ProtocolEncoder &write(float);
    ProtocolEncoder &write(double);
    ProtocolEncoder &write(const char *);
    ProtocolEncoder &write(bool);
    ProtocolEncoder &write(std::vector<uint8_t> &);
    ProtocolEncoder &write(const std::string &);
    ProtocolEncoder &writeArrayStart();
    ProtocolEncoder &writeArrayEnd();
    ProtocolEncoder &writeArray(uint64_t);
    ProtocolEncoder &writeMapStart();
    ProtocolEncoder &writeMapEnd();
    ProtocolEncoder &writeMap(uint64_t);
    ProtocolEncoder &writeNull();
    ProtocolEncoder &writeUndefined();
    ProtocolEncoder &writeBreak();
    ProtocolEncoder &write(char);
    ProtocolEncoder &writeTag(uint64_t);

    inline uint8_t *buffer() { return _buffer; }
    inline uint32_t size() { return _index; }
    inline void error(int x)
    {
        _error = x;
    }
    inline bool ok() { return _error == 0; }

private:
    uint8_t *_buffer;
    uint32_t _capacity;
    uint32_t _index;
    int _error;
    Fcs _fcs;
    void addByte(uint8_t value);
    void addEscaped(uint8_t);
    void addEscaped(uint8_t *, uint32_t);
    void write_type_and_value(uint8_t, uint64_t);
};

//====================================================================================
class CborHeader
{
public:
    bool is_null() const { return hdr == CborNullType; }
    bool is_undefined() const { return hdr == CborUndefinedType; }
    bool is_bool() const { return hdr == 0xf4 || hdr == 0xf5; }
    bool is_break() const { return hdr == 0xff; }
    bool is_bytes() const { return hdr >> 5u == 2; }
    bool is_string() const { return hdr >> 5u == 3; }
    bool is_array() const { return hdr >> 5u == 4; }
    bool is_indefinite_array() const { return hdr == 0x9f; }
    bool is_map() const { return hdr >> 5u == 5; }
    bool is_indefinite_map() const { return hdr == 0xb6; }
    bool is_tag() const { return hdr >> 5u == 6; }
    bool is_float() const { return hdr == CborFloatType; }
    bool is_double() const { return hdr == CborDoubleType; }
    bool is_uint() const { return hdr >> 5u == 0; }
    bool is_int() const { return hdr >> 5u == 1; }
    bool as_bool() const
    {
        if (hdr == 0xf4)
            return false;
        return true;
    }

    uint64_t as_uint() const
    {
        return val;
    }

    int64_t as_int() const
    {
        if (hdr >> 5u == 0)
            return val;
        if (hdr >> 5u != 1)
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

    uint8_t hdr = 0;
    uint64_t val = 0;
};

class ProtocolDecoder
{
public:
    ProtocolDecoder(uint32_t);
    void reset();
    bool checkCrc();
    void addUnEscaped(uint8_t);
    void addUnEscaped(const std::vector<uint8_t>);
    uint8_t *buffer() { return _buffer; }
    uint32_t size() { return _writePtr; }
    ProtocolDecoder &rewind();
    uint8_t get_byte();
    void put_byte(uint8_t);
    void put_bytes(const uint8_t *, uint32_t);

    ProtocolDecoder &readArrayStart();
    ProtocolDecoder &readArrayEnd();
    ProtocolDecoder &readMapStart();
    ProtocolDecoder &readMapEnd();
    ProtocolDecoder &read(bool &);
    ProtocolDecoder &read(int &);
    ProtocolDecoder &read(unsigned int &);
    ProtocolDecoder &read(long &);
    ProtocolDecoder &read(unsigned long &);
    ProtocolDecoder &read(long long &);
    ProtocolDecoder &read(unsigned long long &);
    ProtocolDecoder &read(float &);
    ProtocolDecoder &read(double &);
    ProtocolDecoder &read(char *, uint32_t);
    ProtocolDecoder &read(std::string &);
    CborHeader peek();
    inline void error(int x)
    {
        _error = x;
    }
    inline bool ok() { return _error == 0; }

private:
    uint8_t *_buffer;
    uint32_t _capacity;
    uint32_t _writePtr;
    uint32_t _readPtr;
    Fcs _fcs;
    int _error;
    CborHeader _h;
    bool next();
};
