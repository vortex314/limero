#include <Protocol.h>

//==============================================================================
static const uint16_t fcsTable[256] = {
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF, 0x8C48,
    0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7, 0x1081, 0x0108,
    0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E, 0x9CC9, 0x8D40, 0xBFDB,
    0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876, 0x2102, 0x308B, 0x0210, 0x1399,
    0x6726, 0x76AF, 0x4434, 0x55BD, 0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E,
    0xFAE7, 0xC87C, 0xD9F5, 0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E,
    0x54B5, 0x453C, 0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD,
    0xC974, 0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3, 0x5285,
    0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A, 0xDECD, 0xCF44,
    0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72, 0x6306, 0x728F, 0x4014,
    0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9, 0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5,
    0xA96A, 0xB8E3, 0x8A78, 0x9BF1, 0x7387, 0x620E, 0x5095, 0x411C, 0x35A3,
    0x242A, 0x16B1, 0x0738, 0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862,
    0x9AF9, 0x8B70, 0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E,
    0xF0B7, 0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036, 0x18C1,
    0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E, 0xA50A, 0xB483,
    0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5, 0x2942, 0x38CB, 0x0A50,
    0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD, 0xB58B, 0xA402, 0x9699, 0x8710,
    0xF3AF, 0xE226, 0xD0BD, 0xC134, 0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7,
    0x6E6E, 0x5CF5, 0x4D7C, 0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1,
    0xA33A, 0xB2B3, 0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72,
    0x3EFB, 0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A, 0xE70E,
    0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1, 0x6B46, 0x7ACF,
    0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9, 0xF78F, 0xE606, 0xD49D,
    0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330, 0x7BC7, 0x6A4E, 0x58D5, 0x495C,
    0x3DE3, 0x2C6A, 0x1EF1, 0x0F78};

bool Fcs::write(uint8_t b)
{
    _fcs = (_fcs >> 8) ^ fcsTable[(_fcs & 0xFF) ^ b];
    return true;
}
//=============================================================================
ProtocolEncoder::ProtocolEncoder(uint32_t size)
{
 //   auto pui = new uint32_t[(size / 4) + 1]; // allocate space for the array in 4 byte words
    reserve(size);
  //  _buffer = (uint8_t *)pui;
    _capacity = size;
}

ProtocolEncoder &ProtocolEncoder::start()
{
    _error = 0;
    _fcs.clear();
    clear();
    addByte(PPP_FLAG_CHAR);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::end()
{
    uint16_t fcs = _fcs.result(); // addEscaped influences the result
    addEscaped(fcs & 0xFF);       // LSB first
    addEscaped(fcs >> 8);
    addByte(PPP_FLAG_CHAR);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::writeNull()
{
    addEscaped(0xf6);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::writeUndefined()
{
    addEscaped(0xf7);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::write(bool value)
{
    addEscaped(value ? 0xf5 : 0xf4);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::writeBreak()
{
    addEscaped(0xff);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::write(uint64_t value)
{
    write_type_and_value(0, value);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::write(uint32_t value)
{
    return write((uint64_t)value);
}

ProtocolEncoder &ProtocolEncoder::write(int value)
{
    return write((int64_t)value);
}

ProtocolEncoder &ProtocolEncoder::write(int32_t value)
{
    return write((int64_t)value);
}

ProtocolEncoder &ProtocolEncoder::write(int64_t value)
{
    if (value < 0)
        write_type_and_value(1, -(value + 1));
    else
        write_type_and_value(0, value);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::write(char c)
{
    switch (c)
    {
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

ProtocolEncoder &ProtocolEncoder::write(std::vector<uint8_t> &bs)
{
    write_type_and_value(2, bs.size());
    for (size_t i = 0; i < bs.size(); i++)
        addEscaped(bs[i]);
    return *this;
}

void ProtocolEncoder::write_type_and_value(uint8_t major_type, uint64_t value)
{
    major_type <<= 5u;
    if (value < 24)
    {
        addEscaped((uint8_t)(major_type | value));
    }
    else if (value < 256)
    {
        addEscaped((uint8_t)(major_type | 24u));
        addEscaped((uint8_t)value);
    }
    else if (value < 65536)
    {
        addEscaped((uint8_t)(major_type | 25u));
        addEscaped((uint8_t)(value >> 8u));
        addEscaped((uint8_t)value);
    }
    else if (value < 4294967296ULL)
    {
        addEscaped((uint8_t)(major_type | 26u));
        addEscaped((uint8_t)(value >> 24u));
        addEscaped((uint8_t)(value >> 16u));
        addEscaped((uint8_t)(value >> 8u));
        addEscaped((uint8_t)value);
    }
    else
    {
        addEscaped((uint8_t)(major_type | 27u));
        addEscaped((uint8_t)(value >> 56u));
        addEscaped((uint8_t)(value >> 48u));
        addEscaped((uint8_t)(value >> 40u));
        addEscaped((uint8_t)(value >> 32u));
        addEscaped((uint8_t)(value >> 24u));
        addEscaped((uint8_t)(value >> 16u));
        addEscaped((uint8_t)(value >> 8u));
        addEscaped((uint8_t)(value));
    }
}

ProtocolEncoder &ProtocolEncoder::write(const std::string &s)
{
    write_type_and_value(3, s.size());
    for (size_t i = 0; i < s.size(); i++)
        addEscaped(s.c_str()[i]);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::write(const char *s)
{
    size_t size = strlen(s);
    write_type_and_value(3, size);
    for (size_t i = 0; i < size; i++)
        addEscaped(s[i]);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::writeArrayStart()
{
    addEscaped(0x9f);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::writeArrayEnd()
{
    return writeBreak();
}

ProtocolEncoder &ProtocolEncoder::writeArray(uint64_t size)
{
    write_type_and_value(4, size);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::writeMap(uint64_t size)
{
    write_type_and_value(5, size);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::writeMapStart()
{
    addEscaped(0xbf);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::writeMapEnd()
{
    return writeBreak();
}

ProtocolEncoder &ProtocolEncoder::writeTag(uint64_t tag)
{
    write_type_and_value(6, tag);
    return *this;
}

ProtocolEncoder &ProtocolEncoder::write(float d)
{
    addEscaped(CborFloatType);
    uint8_t *pb = (uint8_t *)&d;
    for (int i = 0; i < 4; i++)
        addEscaped(*(pb + 3 - i));
    return *this;
}

ProtocolEncoder &ProtocolEncoder::write(double d)
{
    addEscaped(CborDoubleType);
    uint8_t *pb = (uint8_t *)&d;
    for (int i = 0; i < 8; i++)
        addEscaped(*(pb + 7 - i));
    return *this;
}

void ProtocolEncoder::addEscaped(uint8_t *buffer, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        addEscaped(buffer[i]);
}

void ProtocolEncoder::addEscaped(uint8_t value)
{
    _fcs.write(value);
    if (value == PPP_ESC_CHAR || value == PPP_FLAG_CHAR)
    { // byte stuffing
        addByte(PPP_ESC_CHAR);
        addByte(value ^ PPP_MASK_CHAR);
    }
    else
    {
        addByte(value);
    }
}
void ProtocolEncoder::addByte(uint8_t value)
{
    if (size() + 1 > _capacity)
    {
        _error = ENOMEM;
        return;
    }
    push_back(value);
}

//===============================================================

ProtocolDecoder::ProtocolDecoder(uint32_t size)
{
   // _buffer = new uint8_t[size];
   reserve(size);
    _capacity = size;
   // _writePtr = 0;
}

void ProtocolDecoder::reset()
{
    clear();
    _error = 0;
    _readPtr = 0;
}

ProtocolDecoder &ProtocolDecoder::rewind()
{
    _error = 0;
    _readPtr = 0;
    return *this;
}

void ProtocolDecoder::addUnEscaped(uint8_t c)
{
    static bool escFlag = false;
    if (size() + 1 > _capacity)
    {
        _error = ENOMEM;
        return;
    }
    if (escFlag)
    {
        escFlag = false;
        push_back( c ^ PPP_MASK_CHAR);
    }
    else
    {
        if (c == PPP_ESC_CHAR)
        {
            escFlag = true;
        }
        else
        {
            push_back( c);
        }
    }
}

bool ProtocolDecoder::next()
{
    _h.hdr = get_byte();
    if (!ok())
        return false;
    const uint8_t t = _h.hdr & 31u;

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

bool ProtocolDecoder::checkCrc()
{

    Fcs fcs;
    for (uint32_t i = 0; i < size(); i++)
    {
        fcs.write(at(i));
    }
    if (fcs.result() == 0x0F47)
    {
        pop_back();
        pop_back();
        return true;
    }
    return false;
}

ProtocolDecoder &ProtocolDecoder::readArrayStart()
{
    if (ok() && next() && _h.is_array())
    {
    }
    else
        error(EPROTO);
    return *this;
}

ProtocolDecoder &ProtocolDecoder::readArrayEnd()
{
    if (ok() && next() && _h.is_break())
    {
    }
    else
        error(EPROTO);
    return *this;
}

ProtocolDecoder &ProtocolDecoder::readMapEnd()
{
    return readArrayEnd();
}

ProtocolDecoder &ProtocolDecoder::readMapStart()
{
    if (ok() && next() && _h.is_map())
    {
    }
    else
        error(EPROTO);
    return *this;
}
// check if string is expected value
ProtocolDecoder &ProtocolDecoder::read(const char *s)
{
    std::string str;
    if (read(str).ok() && s == str)
        return *this;
    error(EINVAL);
    return *this;
}

// check if string is expected value
ProtocolDecoder &ProtocolDecoder::read(const char c)
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

ProtocolDecoder &ProtocolDecoder::read(std::string &s)
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

ProtocolDecoder &ProtocolDecoder::read(float &d)
{
    if (ok() && next() && _h.is_float())
    {
        union
        {
            float f;
            uint8_t b[4];
        } u;
        for (uint32_t i = 0; i < 4; i++)
            u.b[3 - i] = get_byte();
        d = u.f;
    }
    else
        error(EPROTO);
    return *this;
}

ProtocolDecoder &ProtocolDecoder::read(uint64_t &ui)
{
    if (ok() && next() && _h.is_uint())
    {
        ui = _h.val;
    }
    else
        error(EPROTO);
    return *this;
}

ProtocolDecoder &ProtocolDecoder::read(uint32_t &ui)
{
    uint64_t ui64 = 0;
    read(ui64);
    ui = ui64;
    return *this;
}

ProtocolDecoder &ProtocolDecoder::read(int64_t &i)
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

ProtocolDecoder &ProtocolDecoder::read(int32_t &i)
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

ProtocolDecoder &ProtocolDecoder::read(bool &b)
{
    if (ok() && next() && (_h.is_bool()))
    {
        b = _h.as_bool();
    }
    else
        error(EPROTO);
    return *this;
}

uint8_t ProtocolDecoder::get_byte()
{
    if (_readPtr < size())
    {
        return at(_readPtr++);
    };
    return 0;
}

void ProtocolDecoder::put_byte(uint8_t b)
{
    if (size() < _capacity)
        push_back(b);
    else
        error(ENOMEM);
}

void ProtocolDecoder::put_bytes(const uint8_t *b, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        put_byte(b[i]);
}

