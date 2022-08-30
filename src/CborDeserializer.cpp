#include "CborDeserializer.h"

#include <Log.h>

#undef assert
#define assert(expr) \
  if (!(expr)) WARN(" assert failed :" #expr);

CborDeserializer::CborDeserializer(size_t size) {
  _buffer = new uint8_t[size];
  _capacity = size;
}
CborDeserializer::~CborDeserializer() { delete[] _buffer; }

CborDeserializer &CborDeserializer::begin() {
  _cborError = CborNoError;
  _error = 0;

  _currentValue = new CborValue;
  _cborError = cbor_parser_init(_buffer, _size, 0, &_decoder, _currentValue);
  if (_cborError != CborNoError) _error = EPROTO;
  return *this;
}

CborDeserializer &CborDeserializer::startArray() {
  if (ok() && cbor_value_is_container(_currentValue) &&
      cbor_value_is_array(_currentValue)) {
    auto containerValue = new CborValue;
    _cborError = cbor_value_enter_container(_currentValue, containerValue);
    if (_cborError != CborNoError) {
      _error = EPROTO;
      delete containerValue;
    } else {
      _stack.push_back(_currentValue);
      _currentValue = containerValue;
    }
  } else {
    _error = EPROTO;
  }
  return *this;
};

CborDeserializer &CborDeserializer::endArray() {
  if (_stack.size() > 1) {
    auto prevContainer = _stack.back();
    _stack.pop_back();
    if (ok() && cbor_value_leave_container(prevContainer, _currentValue) ==
                    CborNoError) {
      delete _currentValue;
      _currentValue = prevContainer;
    }
  } else {
    _error = EPROTO;
  }
  return *this;
};

CborDeserializer &CborDeserializer::startMap() {
  if (ok() && cbor_value_is_container(_currentValue) &&
      cbor_value_is_map(_currentValue)) {
    auto containerValue = new CborValue;
    _cborError = cbor_value_enter_container(_currentValue, containerValue);
    if (_cborError != CborNoError) {
      _error = EPROTO;
      delete containerValue;
    } else {
      _stack.push_back(_currentValue);
      _currentValue = containerValue;
    }
  } else {
    _error = EPROTO;
  }
  return *this;
};

CborDeserializer &CborDeserializer::endMap() {
  if (_stack.size() > 1) {
    auto prevContainer = _stack.back();
    _stack.pop_back();
    if (ok() && cbor_value_leave_container(prevContainer, _currentValue) ==
                    CborNoError) {
      delete _currentValue;
      _currentValue = prevContainer;
    }
  } else {
    _error = EPROTO;
  }
  return *this;
};

CborDeserializer &CborDeserializer::end() {
  for (auto v : _stack) {
    delete v;
  }
  _stack.clear();
  delete _currentValue;
  return *this;
};

CborDeserializer &CborDeserializer::get(Bytes &t) {
  size_t size;
  if (!_cborError && cbor_value_is_byte_string(_currentValue) &&
      cbor_value_calculate_string_length(_currentValue, &size) == 0) {
    uint8_t *temp;
    size_t size;
    _cborError = cbor_value_dup_byte_string(_currentValue, &temp, &size, 0);
    assert(_cborError == CborNoError);
    t = Bytes(temp, temp + size);
    free(temp);
    if (!_cborError) _cborError = cbor_value_advance(_currentValue);
  } else {
    _cborError = CborErrorIllegalType;
  }

  return *this;
}

CborDeserializer &CborDeserializer::get(std::string &t) {
  if (ok() && cbor_value_is_text_string(_currentValue)) {
    char *temp;
    size_t size;
    _cborError = cbor_value_dup_text_string(_currentValue, &temp, &size, 0);
    if (_cborError != CborNoError) _error = EPROTO;
    t = temp;
    ::free(temp);
    if (ok()) _cborError = cbor_value_advance(_currentValue);
  } else {
    _cborError = CborErrorIllegalType;
  }
  return *this;
}

CborDeserializer &CborDeserializer::get(int &t) {
  if (!_cborError && cbor_value_is_integer(_currentValue)) {
    _cborError = cbor_value_get_int(_currentValue, &t);
    if (!_cborError) _cborError = cbor_value_advance_fixed(_currentValue);
  } else {
    _cborError = CborErrorIllegalType;
  }

  return *this;
}

CborDeserializer &CborDeserializer::get(int64_t &t) {
  if (!_cborError && cbor_value_is_integer(_currentValue)) {
    _cborError = cbor_value_get_int64(_currentValue, &t);
    if (!_cborError) _cborError = cbor_value_advance(_currentValue);
  } else {
    _cborError = CborErrorIllegalType;
  }

  return *this;
}

CborDeserializer &CborDeserializer::get(uint64_t &t) {
  if (!_cborError && cbor_value_is_unsigned_integer(_currentValue)) {
    _cborError = cbor_value_get_uint64(_currentValue, &t);
    if (!_cborError) _cborError = cbor_value_advance_fixed(_currentValue);
  } else {
    _cborError = CborErrorIllegalType;
  }

  return *this;
}

CborDeserializer &CborDeserializer::get(double &t) {
  if (!_cborError && cbor_value_is_double(_currentValue)) {
    _cborError = cbor_value_get_double(_currentValue, &t);
    if (!_cborError) _cborError = cbor_value_advance(_currentValue);
  } else {
    _cborError = CborErrorIllegalType;
  }

  return *this;
}

CborDeserializer &CborDeserializer::get(const int &t) {
  int x;
  if (!_cborError && cbor_value_is_integer(_currentValue)) {
    _cborError = cbor_value_get_int(_currentValue, &x);
    assert(_cborError == CborNoError);
    if (!_cborError) _cborError = cbor_value_advance_fixed(_currentValue);
  } else {
    _cborError = CborErrorIllegalType;
  }
  return *this;
}

CborDeserializer &CborDeserializer::fromBytes(const Bytes &bs) {
  assert(bs.size() < _capacity);
  _size = bs.size() < _capacity ? bs.size() : _capacity;
  memcpy(_buffer, bs.data(), _size);
  return *this;
};
