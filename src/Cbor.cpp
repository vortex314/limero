#include <Cbor.h>


Special Start(1);
Special End(2);
Special Break(3);

CborEncoder::CborEncoder(uint32_t _size)
{
    //   auto pui = new uint32_t[(size / 4) + 1]; // allocate space for the array in 4 byte words
    reserve(_size);
    //  _buffer = (uint8_t *)pui;
    _capacity = _size;
}

CborEncoder &CborEncoder::start()
{
    _error = 0;
    clear();
    return *this;
}

CborEncoder &CborEncoder::end()
{
    return *this;
}

CborEncoder &CborEncoder::writeNull()
{
    push_back(0xf6);
    return *this;
}

CborEncoder &CborEncoder::writeUndefined()
{
    push_back(0xf7);
    return *this;
}

CborEncoder &CborEncoder::write(bool value)
{
    push_back(value ? 0xf5 : 0xf4);
    return *this;
}

CborEncoder &CborEncoder::writeBreak()
{
    push_back(0xff);
    return *this;
}

CborEncoder &CborEncoder::write(uint64_t value)
{
    write_type_and_value(0, value);
    return *this;
}

CborEncoder &CborEncoder::write(uint32_t value)
{
    return write((uint64_t)value);
}
/*
CborEncoder &CborEncoder::write(int value)
{
    return write((int64_t)value);
}*/

CborEncoder &CborEncoder::write(int32_t value)
{
    return write((int64_t)value);
}

CborEncoder &CborEncoder::write(int64_t value)
{
    if (value < 0)
        write_type_and_value(1, -(value + 1));
    else
        write_type_and_value(0, value);
    return *this;
}

CborEncoder &CborEncoder::write(char c)
{
    switch (c)
    {
    case '<':
        return start();
    case '>':
        return end();
    case '{':
        return writeMapStart();
    case '}':
        return writeBreak();
    case '[':
        return writeArrayStart();
    case ']':
        return writeBreak();
    }
    return *this;
}

CborEncoder &CborEncoder::write(std::vector<uint8_t> &bs)
{
    write_type_and_value(2, bs.size());
    for (size_t i = 0; i < bs.size(); i++)
        push_back(bs[i]);
    return *this;
}

void CborEncoder::write_type_and_value(uint8_t major_type, uint64_t value)
{
    major_type <<= 5u;
    if (value < 24)
    {
        push_back((uint8_t)(major_type | value));
    }
    else if (value < 256)
    {
        push_back((uint8_t)(major_type | 24u));
        push_back((uint8_t)value);
    }
    else if (value < 65536)
    {
        push_back((uint8_t)(major_type | 25u));
        push_back((uint8_t)(value >> 8u));
        push_back((uint8_t)value);
    }
    else if (value < 4294967296ULL)
    {
        push_back((uint8_t)(major_type | 26u));
        push_back((uint8_t)(value >> 24u));
        push_back((uint8_t)(value >> 16u));
        push_back((uint8_t)(value >> 8u));
        push_back((uint8_t)value);
    }
    else
    {
        push_back((uint8_t)(major_type | 27u));
        push_back((uint8_t)(value >> 56u));
        push_back((uint8_t)(value >> 48u));
        push_back((uint8_t)(value >> 40u));
        push_back((uint8_t)(value >> 32u));
        push_back((uint8_t)(value >> 24u));
        push_back((uint8_t)(value >> 16u));
        push_back((uint8_t)(value >> 8u));
        push_back((uint8_t)(value));
    }
}

CborEncoder &CborEncoder::write(const std::string &s)
{
    write_type_and_value(3, s.size());
    for (size_t i = 0; i < s.size(); i++)
        push_back(s.c_str()[i]);
    return *this;
}

CborEncoder &CborEncoder::write(const char *s)
{
    size_t length = strlen(s);
    write_type_and_value(3, length);
    for (size_t i = 0; i < length; i++)
        push_back(s[i]);
    return *this;
}

CborEncoder &CborEncoder::writeArrayStart()
{
    push_back(0x9f);
    return *this;
}

CborEncoder &CborEncoder::writeArrayEnd()
{
    return writeBreak();
}

CborEncoder &CborEncoder::writeArray(uint64_t __size)
{
    write_type_and_value(4, __size);
    return *this;
}

CborEncoder &CborEncoder::writeMap(uint64_t __size)
{
    write_type_and_value(5, __size);
    return *this;
}

CborEncoder &CborEncoder::writeMapStart()
{
    push_back(0xbf);
    return *this;
}

CborEncoder &CborEncoder::writeMapEnd()
{
    return writeBreak();
}

CborEncoder &CborEncoder::writeTag(uint64_t tag)
{
    write_type_and_value(6, tag);
    return *this;
}

CborEncoder &CborEncoder::write(float d)
{
    push_back(CborFloatType);
    uint8_t *pb = (uint8_t *)&d;
    for (int i = 0; i < 4; i++)
        push_back(*(pb + 3 - i));
    return *this;
}

CborEncoder &CborEncoder::write(double d)
{
    push_back(CborDoubleType);
    uint8_t *pb = (uint8_t *)&d;
    for (int i = 0; i < 8; i++)
        push_back(*(pb + 7 - i));
    return *this;
}





//===============================================================

CborDecoder::CborDecoder(uint32_t __size)
{
    // _buffer = new uint8_t[size];
    reserve(__size);
    _capacity = __size;
    // _writePtr = 0;
}

void CborDecoder::reset()
{
    clear();
    _error = 0;
    _readPtr = 0;
}

CborDecoder &CborDecoder::rewind()
{
    _error = 0;
    _readPtr = 0;
    return *this;
}


bool CborDecoder::next()
{
    _h.firstByte = get_byte();
    if (!ok())
        return false;
    const uint8_t t = _h.firstByte & 31u;

    if (t < 24)
    {
        _h.val = t;
    }
    else if (t == 24)
    {
        _h.val = get_byte();
    }
    else if (t == 25)
    {
        _h.val = get_byte();
        _h.val = (_h.val << 8u) + get_byte();
    }
    else if (t == 26)
    {
        _h.val = get_byte();
        _h.val = (_h.val << 8u) + get_byte();
        _h.val = (_h.val << 8u) + get_byte();
        _h.val = (_h.val << 8u) + get_byte();
    }
    else if (t == 27)
    {
        _h.val = get_byte();
        _h.val = (_h.val << 8u) + get_byte();
        _h.val = (_h.val << 8u) + get_byte();
        _h.val = (_h.val << 8u) + get_byte();
        _h.val = (_h.val << 8u) + get_byte();
        _h.val = (_h.val << 8u) + get_byte();
        _h.val = (_h.val << 8u) + get_byte();
        _h.val = (_h.val << 8u) + get_byte();
    }
    else if (t != 31)
    {
        error(EPROTO);
        return false;
    }

    return true;
}

CborDecoder &CborDecoder::readArrayStart()
{
    if (ok() && next() && _h.is_array())
    {
    }
    else
        error(EPROTO);
    return *this;
}

CborDecoder &CborDecoder::readArrayEnd()
{
    if (ok() && next() && _h.is_break())
    {
    }
    else
        error(EPROTO);
    return *this;
}

CborDecoder &CborDecoder::readMapEnd()
{
    return readArrayEnd();
}

CborDecoder &CborDecoder::readMapStart()
{
    if (ok() && next() && _h.is_map())
    {
    }
    else
        error(EPROTO);
    return *this;
}
// check if string is expected value
CborDecoder &CborDecoder::read(const char *s)
{
    std::string str;
    if (read(str).ok() && s == str)
        return *this;
    error(EINVAL);
    return *this;
}

// check special marker is found
CborDecoder &CborDecoder::read(const char c)
{
    switch (c)
    {
    case '[':
    {
        readArrayStart();
        break;
    }
    case ']':
    {
        readArrayEnd();
        break;
    }
    case '{':
    {
        readMapStart();
        break;
    }
    case '}':
    {
        readMapEnd();
        break;
    }
    }
    return *this;
}

CborDecoder &CborDecoder::read(std::string &s)
{
    if (ok() && next() && _h.is_string())
    {
        s.clear();
        for (uint32_t i = 0; i < _h.val; i++)
            s.push_back(get_byte());
    }
    else
        error(EPROTO);
    return *this;
}

CborDecoder &CborDecoder::read(float &d)
{
    if (ok() && next() && _h.is_float())
    {
        union
        {
            float f;
            uint8_t b[4];
        } output;
#ifdef __LITTLE_ENDIAN__
        output.b[0] = _h.val;
        output.b[1] = _h.val >> 8;
        output.b[2] = _h.val >> 16;
        output.b[3] = _h.val >> 24;
#else
        output.b[3] = _h.val;
        output.b[2] = _h.val >> 8;
        output.b[1] = _h.val >> 16;
        output.b[0] = _h.val >> 24;
#endif
        d = output.f;
    }
    else
        error(EPROTO);
    return *this;
}

CborDecoder &CborDecoder::read(uint64_t &ui)
{
    if (ok() && next() && _h.is_uint())
    {
        ui = _h.val;
    }
    else
        error(EPROTO);
    return *this;
}

CborDecoder &CborDecoder::read(uint32_t &ui)
{
    uint64_t ui64 = 0;
    read(ui64);
    ui = ui64;
    return *this;
}

CborDecoder &CborDecoder::read(int64_t &i)
{
    if (ok() && next() && (_h.is_uint() || _h.is_int()))
    {
        if (_h.is_uint())
            i = _h.val;
        else
            i = -1 - _h.val;
    }
    else
        error(EPROTO);
    return *this;
}

CborDecoder &CborDecoder::read(int32_t &i)
{
    if (ok() && next() && (_h.is_uint() || _h.is_int()))
    {
        if (_h.is_uint())
            i = _h.val;
        else
            i = -1 - _h.val;
    }
    else
        error(EPROTO);
    return *this;
}

CborDecoder &CborDecoder::read(bool &b)
{
    if (ok() && next() && (_h.is_bool()))
    {
        b = _h.as_bool();
    }
    else
        error(EPROTO);
    return *this;
}

uint8_t CborDecoder::get_byte()
{
    if (_readPtr < size())
    {
        return at(_readPtr++);
    };
    return 0;
}

void CborDecoder::put_byte(uint8_t b)
{
    if (size() < _capacity)
        push_back(b);
    else
        error(ENOMEM);
}

void CborDecoder::put_bytes(const Bytes& bs)
{
    for (auto b:bs)
        put_byte(b);
}


CborHeader CborDecoder::peek()
{
    CborHeader hdr;
    uint8_t b = at(_readPtr);
    hdr.firstByte = b;
    return hdr;
}
