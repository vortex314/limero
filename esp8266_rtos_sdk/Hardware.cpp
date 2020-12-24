#include <Hardware.h>
#include <Sys.h>
#include <Log.h>
/*
 * ESP hardware SPI master driver
 *
 * Part of esp-open-rtos
 * Copyright (c) Ruslan V. Uss, 2016
 * BSD Licensed as described in the file LICENSE
 */
#include "driver/gpio.h"
#include "esp/spi.h"
#include "esp/iomux.h"
#include "esp/gpio.h"
#include <string.h>

#define _SPI0_SCK_GPIO 6
#define _SPI0_MISO_GPIO 7
#define _SPI0_MOSI_GPIO 8
#define _SPI0_HD_GPIO 9
#define _SPI0_WP_GPIO 10
#define _SPI0_CS0_GPIO 11

#define _SPI1_MISO_GPIO 12
#define _SPI1_MOSI_GPIO 13
#define _SPI1_SCK_GPIO 14
#define _SPI1_CS0_GPIO 15

#define _SPI0_FUNC IOMUX_FUNC(1)
#define _SPI1_FUNC IOMUX_FUNC(2)

#define _SPI_BUF_SIZE 64
#define __min(a, b) ((a > b) ? (b) : (a))

static bool _minimal_pins[2] = {false, false};

bool spi_init(uint8_t bus, spi_mode_t mode, uint32_t freq_divider, bool msb, spi_endianness_t endianness, bool minimal_pins)
{
  switch (bus)
  {
  case 0:
    gpio_set_iomux_function(_SPI0_MISO_GPIO, _SPI0_FUNC);
    gpio_set_iomux_function(_SPI0_MOSI_GPIO, _SPI0_FUNC);
    gpio_set_iomux_function(_SPI0_SCK_GPIO, _SPI0_FUNC);
    if (!minimal_pins)
    {
      gpio_set_iomux_function(_SPI0_HD_GPIO, _SPI0_FUNC);
      gpio_set_iomux_function(_SPI0_WP_GPIO, _SPI0_FUNC);
      gpio_set_iomux_function(_SPI0_CS0_GPIO, _SPI0_FUNC);
    }
    break;
  case 1:
    gpio_set_iomux_function(_SPI1_MISO_GPIO, _SPI1_FUNC);
    gpio_set_iomux_function(_SPI1_MOSI_GPIO, _SPI1_FUNC);
    gpio_set_iomux_function(_SPI1_SCK_GPIO, _SPI1_FUNC);
    if (!minimal_pins)
      gpio_set_iomux_function(_SPI1_CS0_GPIO, _SPI1_FUNC);
    break;
  default:
    return false;
  }

  _minimal_pins[bus] = minimal_pins;
  SPI(bus).USER0 = SPI_USER0_MOSI | SPI_USER0_CLOCK_IN_EDGE | SPI_USER0_DUPLEX |
                   (minimal_pins ? 0 : (SPI_USER0_CS_HOLD | SPI_USER0_CS_SETUP));

  spi_set_frequency_div(bus, freq_divider);
  spi_set_mode(bus, mode);
  spi_set_msb(bus, msb);
  spi_set_endianness(bus, endianness);

  return true;
}

void spi_get_settings(uint8_t bus, spi_settings_t *s)
{
  s->mode = spi_get_mode(bus);
  s->freq_divider = spi_get_frequency_div(bus);
  s->msb = spi_get_msb(bus);
  s->endianness = spi_get_endianness(bus);
  s->minimal_pins = _minimal_pins[bus];
}

void spi_set_mode(uint8_t bus, spi_mode_t mode)
{
  bool cpha = (uint8_t)mode & 1;
  bool cpol = (uint8_t)mode & 2;
  if (cpol)
    cpha = !cpha; // CPHA must be inverted when CPOL = 1, I have no idea why

  // CPHA
  if (cpha)
    SPI(bus).USER0 |= SPI_USER0_CLOCK_OUT_EDGE;
  else
    SPI(bus).USER0 &= ~SPI_USER0_CLOCK_OUT_EDGE;

  // CPOL - see http://bbs.espressif.com/viewtopic.php?t=342#p5384
  if (cpol)
    SPI(bus).PIN |= SPI_PIN_IDLE_EDGE;
  else
    SPI(bus).PIN &= ~SPI_PIN_IDLE_EDGE;
}

spi_mode_t spi_get_mode(uint8_t bus)
{
  uint8_t cpha = SPI(bus).USER0 & SPI_USER0_CLOCK_OUT_EDGE ? 1 : 0;
  uint8_t cpol = SPI(bus).PIN & SPI_PIN_IDLE_EDGE ? 2 : 0;

  return (spi_mode_t)(cpol | (cpol ? 1 - cpha : cpha)); // see spi_set_mode
}

void spi_set_msb(uint8_t bus, bool msb)
{
  if (msb)
    SPI(bus).CTRL0 &= ~(SPI_CTRL0_WR_BIT_ORDER | SPI_CTRL0_RD_BIT_ORDER);
  else
    SPI(bus).CTRL0 |= (SPI_CTRL0_WR_BIT_ORDER | SPI_CTRL0_RD_BIT_ORDER);
}

void spi_set_endianness(uint8_t bus, spi_endianness_t endianness)
{
  if (endianness == SPI_BIG_ENDIAN)
    SPI(bus).USER0 |= (SPI_USER0_WR_BYTE_ORDER | SPI_USER0_RD_BYTE_ORDER);
  else
    SPI(bus).USER0 &= ~(SPI_USER0_WR_BYTE_ORDER | SPI_USER0_RD_BYTE_ORDER);
}

void spi_set_frequency_div(uint8_t bus, uint32_t divider)
{
  uint32_t predivider = (divider & 0xffff) - 1;
  uint32_t count = (divider >> 16) - 1;
  if (count || predivider)
  {
    IOMUX.CONF &= ~(bus == 0 ? IOMUX_CONF_SPI0_CLOCK_EQU_SYS_CLOCK : IOMUX_CONF_SPI1_CLOCK_EQU_SYS_CLOCK);
    SPI(bus).CLOCK = VAL2FIELD_M(SPI_CLOCK_DIV_PRE, predivider) |
                     VAL2FIELD_M(SPI_CLOCK_COUNT_NUM, count) |
                     VAL2FIELD_M(SPI_CLOCK_COUNT_HIGH, count / 2) |
                     VAL2FIELD_M(SPI_CLOCK_COUNT_LOW, count);
  }
  else
  {
    IOMUX.CONF |= bus == 0 ? IOMUX_CONF_SPI0_CLOCK_EQU_SYS_CLOCK : IOMUX_CONF_SPI1_CLOCK_EQU_SYS_CLOCK;
    SPI(bus).CLOCK = SPI_CLOCK_EQU_SYS_CLOCK;
  }
}

inline static void _set_size(uint8_t bus, uint8_t bytes)
{
  uint32_t bits = ((uint32_t)bytes << 3) - 1;
  SPI(bus).USER1 = SET_FIELD(SPI(bus).USER1, SPI_USER1_MISO_BITLEN, bits);
  SPI(bus).USER1 = SET_FIELD(SPI(bus).USER1, SPI_USER1_MOSI_BITLEN, bits);
}

inline static void _wait(uint8_t bus)
{
  while (SPI(bus).CMD & SPI_CMD_USR)
    ;
}

inline static void _start(uint8_t bus)
{
  SPI(bus).CMD |= SPI_CMD_USR;
}

inline static void _store_data(uint8_t bus, const void *data, size_t len)
{
  uint8_t words = len / 4;
  uint8_t tail = len % 4;

  memcpy((void *)SPI(bus).W, data, len - tail);

  if (!tail)
    return;

  uint32_t last = 0;
  uint8_t *offs = (uint8_t *)data + len - tail;
  for (uint8_t i = 0; i < tail; i++)
    last = last | (offs[i] << (i * 8));
  SPI(bus).W[words] = last;
}

inline static uint32_t _swap_bytes(uint32_t value)
{
  return (value << 24) | ((value << 8) & 0x00ff0000) | ((value >> 8) & 0x0000ff00) | (value >> 24);
}

inline static uint32_t _swap_words(uint32_t value)
{
  return (value << 16) | (value >> 16);
}

static void _spi_buf_prepare(uint8_t bus, size_t len, spi_endianness_t e, spi_word_size_t word_size)
{
  if (e == SPI_LITTLE_ENDIAN || word_size == SPI_32BIT)
    return;

  size_t count = word_size == SPI_16BIT ? (len + 1) / 2 : (len + 3) / 4;
  uint32_t *data = (uint32_t *)SPI(bus).W;
  for (size_t i = 0; i < count; i++)
  {
    data[i] = word_size == SPI_16BIT
                  ? _swap_words(data[i])
                  : _swap_bytes(data[i]);
  }
}

static void _spi_buf_transfer(uint8_t bus, const void *out_data, void *in_data,
                              size_t len, spi_endianness_t e, spi_word_size_t word_size)
{
  _wait(bus);
  size_t bytes = len * (uint8_t)word_size;
  _set_size(bus, bytes);
  _store_data(bus, out_data, bytes);
  _spi_buf_prepare(bus, len, e, word_size);
  _start(bus);
  _wait(bus);
  if (in_data)
  {
    _spi_buf_prepare(bus, len, e, word_size);
    memcpy(in_data, (void *)SPI(bus).W, bytes);
  }
}

uint8_t spi_transfer_8(uint8_t bus, uint8_t data)
{
  uint8_t res;
  _spi_buf_transfer(bus, &data, &res, 1, spi_get_endianness(bus), SPI_8BIT);
  return res;
}

uint16_t spi_transfer_16(uint8_t bus, uint16_t data)
{
  uint16_t res;
  _spi_buf_transfer(bus, &data, &res, 1, spi_get_endianness(bus), SPI_16BIT);
  return res;
}

uint32_t spi_transfer_32(uint8_t bus, uint32_t data)
{
  uint32_t res;
  _spi_buf_transfer(bus, &data, &res, 1, spi_get_endianness(bus), SPI_32BIT);
  return res;
}

static void _rearm_extras_bit(uint8_t bus, bool arm)
{
  if (!_minimal_pins[bus])
    return;
  static uint8_t status[2];

  if (arm)
  {
    if (status[bus] & 0x01)
      SPI(bus).USER0 |= (SPI_USER0_ADDR);
    if (status[bus] & 0x02)
      SPI(bus).USER0 |= (SPI_USER0_COMMAND);
    if (status[bus] & 0x04)
      SPI(bus).USER0 |= (SPI_USER0_DUMMY | SPI_USER0_MISO);
    status[bus] = 0;
  }
  else
  {
    if (SPI(bus).USER0 & SPI_USER0_ADDR)
    {
      SPI(bus).USER0 &= ~(SPI_USER0_ADDR);
      status[bus] |= 0x01;
    }
    if (SPI(bus).USER0 & SPI_USER0_COMMAND)
    {
      SPI(bus).USER0 &= ~(SPI_USER0_COMMAND);
      status[bus] |= 0x02;
    }
    if (SPI(bus).USER0 & SPI_USER0_DUMMY)
    {
      SPI(bus).USER0 &= ~(SPI_USER0_DUMMY | SPI_USER0_MISO);
      status[bus] |= 0x04;
    }
  }
}

size_t spi_transfer(uint8_t bus, const void *out_data, void *in_data, size_t len, spi_word_size_t word_size)
{
  if (!out_data || !len)
    return 0;

  spi_endianness_t e = spi_get_endianness(bus);
  uint8_t buf_size = _SPI_BUF_SIZE / (uint8_t)word_size;

  size_t blocks = len / buf_size;
  for (size_t i = 0; i < blocks; i++)
  {
    size_t offset = i * _SPI_BUF_SIZE;
    _spi_buf_transfer(bus, (const uint8_t *)out_data + offset,
                      in_data ? (uint8_t *)in_data + offset : NULL, buf_size, e, word_size);
    _rearm_extras_bit(bus, false);
  }

  uint8_t tail = len % buf_size;
  if (tail)
  {
    _spi_buf_transfer(bus, (const uint8_t *)out_data + blocks * _SPI_BUF_SIZE,
                      in_data ? (uint8_t *)in_data + blocks * _SPI_BUF_SIZE : NULL, tail, e, word_size);
  }

  if (blocks)
    _rearm_extras_bit(bus, true);
  return len;
}

static void _spi_buf_read(uint8_t bus, uint8_t b, void *in_data,
                          size_t len, spi_endianness_t e, spi_word_size_t word_size)
{
  _wait(bus);
  size_t bytes = len * (uint8_t)word_size;
  _set_size(bus, bytes);
  uint32_t w = ((uint32_t)b << 24) | ((uint32_t)b << 16) | ((uint32_t)b << 8) | b;
  for (uint8_t i = 0; i < _SPI_BUF_SIZE / 4; i++)
    SPI(bus).W[i] = w;
  _start(bus);
  _wait(bus);
  _spi_buf_prepare(bus, len, e, word_size);
  memcpy(in_data, (void *)SPI(bus).W, bytes);
}

void spi_read(uint8_t bus, uint8_t out_byte, void *in_data, size_t len, spi_word_size_t word_size)
{
  spi_endianness_t e = spi_get_endianness(bus);
  uint8_t buf_size = _SPI_BUF_SIZE / (uint8_t)word_size;

  size_t blocks = len / buf_size;
  for (size_t i = 0; i < blocks; i++)
  {
    size_t offset = i * _SPI_BUF_SIZE;
    _spi_buf_read(bus, out_byte, (uint8_t *)in_data + offset, buf_size, e, word_size);
    _rearm_extras_bit(bus, false);
  }

  uint8_t tail = len % buf_size;
  if (tail)
    _spi_buf_read(bus, out_byte, (uint8_t *)in_data + blocks * _SPI_BUF_SIZE, tail, e, word_size);

  if (blocks)
    _rearm_extras_bit(bus, true);
}

static void _repeat_send(uint8_t bus, uint32_t *dword, int32_t *repeats,
                         spi_word_size_t size)
{
  uint8_t i = 0;
  while (*repeats > 0)
  {
    uint16_t bytes_to_transfer = __min(*repeats * size, _SPI_BUF_SIZE);
    _wait(bus);
    if (i)
      _rearm_extras_bit(bus, false);
    _set_size(bus, bytes_to_transfer);
    for (i = 0; i < (bytes_to_transfer + 3) / 4; i++)
      SPI(bus).W[i] = *dword; //need test with memcpy !
    _start(bus);
    *repeats -= (bytes_to_transfer / size);
  }
  _wait(bus);
  _rearm_extras_bit(bus, true);
}

void spi_repeat_send_8(uint8_t bus, uint8_t data, int32_t repeats)
{
  uint32_t dword = data << 24 | data << 16 | data << 8 | data;
  _repeat_send(bus, &dword, &repeats, SPI_8BIT);
}

void spi_repeat_send_16(uint8_t bus, uint16_t data, int32_t repeats)
{
  uint32_t dword = data << 16 | data;
  _repeat_send(bus, &dword, &repeats, SPI_16BIT);
}

void spi_repeat_send_32(uint8_t bus, uint32_t data, int32_t repeats)
{
  _repeat_send(bus, &data, &repeats, SPI_32BIT);
}

#define CLOCK_80MHZ 80000000

class SPI_ESP8266 : public Spi
{
  FunctionPointer _fp;
  void *_arg;
  uint32_t _clock;
  SpiMode _mode;
  bool _lsbFirst;
  bool _hwSelect;
  PhysicalPin _miso, _mosi, _sck, _cs;

public:
  SPI_ESP8266(PhysicalPin miso, PhysicalPin mosi, PhysicalPin sck, PhysicalPin cs)
  {
    _clock = 1000000;
    _mode = SPI_MODE_PHASE0_POL0;
    _hwSelect = true;
    _lsbFirst = false;
    _cs = cs;
    _sck = sck;
    _mosi = mosi;
    _miso = miso;
  }
  ~SPI_ESP8266() {}
  int init()
  {
    spi_settings_t settings;
    BZERO(settings);
    settings.mode = (spi_mode_t)_mode;
    settings.msb = !_lsbFirst;
    if (_clock == SPI_CLOCK_125K)
      settings.freq_divider = SPI_FREQ_DIV_125K;
    else if (_clock == SPI_CLOCK_250K)
      settings.freq_divider = SPI_FREQ_DIV_250K;
    else if (_clock == SPI_CLOCK_500K)
      settings.freq_divider = SPI_FREQ_DIV_500K;
    else if (_clock == SPI_CLOCK_1M)
      settings.freq_divider = SPI_FREQ_DIV_1M;
    else if (_clock == SPI_CLOCK_2M)
      settings.freq_divider = SPI_FREQ_DIV_2M;
    else if (_clock == SPI_CLOCK_4M)
      settings.freq_divider = SPI_FREQ_DIV_4M;
    else if (_clock == SPI_CLOCK_10M)
      settings.freq_divider = SPI_FREQ_DIV_10M;
    else if (_clock == SPI_CLOCK_8M)
      settings.freq_divider = SPI_FREQ_DIV_8M;
    else if (_clock == SPI_CLOCK_20M)
      settings.freq_divider = SPI_FREQ_DIV_20M;
    else
      settings.freq_divider = SPI_FREQ_DIV_1M;
    settings.endianness = SPI_LITTLE_ENDIAN;
    settings.minimal_pins = _hwSelect ? false : true;
    spi_set_settings(1, &settings);
    spi_clear_address(1);
    spi_clear_command(1);
    spi_clear_dummy(1);
    /*       uint32_t spi_ctrl2=0;
	       spi_ctrl2 = READ_PERI_REG(SPI_CTRL2(1));
	       spi_ctrl2 += ( 3 << SPI_MISO_DELAY_NUM_S );
	       spi_ctrl2 += (1 << SPI_MISO_DELAY_MODE_S); // add delay for going trough mux , see ESP8266 ref manual
	       WRITE_PERI_REG(SPI_CTRL2(1), spi_ctrl2);*/

    /*    uint32_t spi_user=READ_PERI_REG(SPI_USER(1));
	    spi_user  |= (SPI_USER0_CS_HOLD | SPI_USER0_CS_SETUP);
	    WRITE_PERI_REG(SPI_USER(1), spi_user);*/
    return 0;
  };
  int deInit() { return 0; }
  // Black magic, for one reason or another the ESP8266 is always 1 bit off !
  int exchange(std::string &in, std::string &out)
  {
    uint8_t inData[out.length()];
    spi_transfer(1, out.data(),inData, out.length(), SPI_8BIT);
    bool lowBit = 0;
    in.clear();
    for (uint32_t i = 0; i < out.length(); i++)
    {
      uint8_t b = inData[i];
      if (lowBit)
        in+= (char)((b >> 1) + 0x80);
      else
        in+=(char) (b >> 1);
      lowBit = b & 1;
    }

    return 0;
  }
  int onExchange(FunctionPointer fp, void *arg)
  {
    _fp = fp;
    _arg = arg;
    return 0;
  }
  int setClock(uint32_t clock)
  {
    _clock = clock;
    return 0;
  }
  int setMode(SpiMode mode)
  {
    _mode = mode;
    return 0;
  }
  int setLsbFirst(bool lsbFirst)
  {
    _lsbFirst = lsbFirst;
    return 0;
  }
  int setHwSelect(bool hwSelect)
  {
    _hwSelect = hwSelect;
    return 0;
  }
};

Spi &Spi::create(PhysicalPin miso, PhysicalPin mosi, PhysicalPin sck, PhysicalPin cs)
{
  Spi *ptr = new SPI_ESP8266(miso, mosi, sck, cs);
  return *ptr;
}


/**********************************************************************
 *
 ######                                                    ###
 #     #     #     ####      #     #####    ##    #         #     #    #
 #     #     #    #    #     #       #     #  #   #         #     ##   #
 #     #     #    #          #       #    #    #  #         #     # #  #
 #     #     #    #  ###     #       #    ######  #         #     #  # #
 #     #     #    #    #     #       #    #    #  #         #     #   ##
 ######      #     ####      #       #    #    #  ######   ###    #    #

 *
 * *******************************************************************/
//================================================== DigitalIn =====
class DigitalIn_ESP8266 : public DigitalIn {
private:
  PhysicalPin _gpio;
  Mode _mode = DIN_PULL_UP;
  void *_object = 0;
  FunctionPointer _fp = 0;
  PinChange _pinChange;
  static bool _isrServiceInstalled;

public:
  DigitalIn_ESP8266(PhysicalPin pin) : _gpio(pin), _pinChange(DIN_NONE){};

  virtual ~DigitalIn_ESP8266(){};

  static DigitalIn &create(PhysicalPin pin);

  int read() { return gpio_get_level((gpio_num_t)_gpio); }

  int init() {
    esp_err_t erc;
    //       INFO(" DigitalIn Init %d ", _gpio);
    erc = gpio_set_direction((gpio_num_t)_gpio, GPIO_MODE_INPUT);
    if (erc)
      ERROR("gpio_set_direction():%d", erc);
    // interrupt of rising edge
    gpio_config_t io_conf;
    ZERO(io_conf);

    gpio_int_type_t interruptType = (gpio_int_type_t)GPIO_INTR_DISABLE;
    if (_pinChange == DIN_RAISE) {
      interruptType = (gpio_int_type_t)GPIO_INTR_POSEDGE;
    } else if (_pinChange == DIN_FALL) {
      interruptType = (gpio_int_type_t)GPIO_INTR_NEGEDGE;
    } else if (_pinChange == DIN_CHANGE) {
      interruptType = (gpio_int_type_t)GPIO_INTR_ANYEDGE;
    }
    INFO(" interrupType : %d ", interruptType);
    io_conf.intr_type = interruptType;
    io_conf.pin_bit_mask = ((uint64_t)1ULL << _gpio); // bit mask of the pins
    io_conf.mode = GPIO_MODE_INPUT;                   // set as input mode
    io_conf.pull_up_en =
        (gpio_pullup_t)(_mode == DIN_PULL_UP ? 1 : 0); // enable pull-up mode
    io_conf.pull_down_en = (gpio_pulldown_t)(_mode == DIN_PULL_DOWN ? 1 : 0);

    //#define ESP_INTR_FLAG_DEFAULT 0
    if (_fp) {
      // install gpio isr service
      if (!_isrServiceInstalled) {
        erc = gpio_install_isr_service(0);
        INFO(" gpio_install_isr_service() ", erc);
        if (erc)
          ERROR("gpio_install_isr_service() = %d", erc);
        _isrServiceInstalled = erc == 0;
      }
      // hook isr handler for specific gpio pin
      erc = gpio_isr_handler_add((gpio_num_t)_gpio, _fp, (void *)_object);
      INFO(" gpio_isr_handler_add(%d,0x%X,0x%X) = %d ", _gpio, _fp, _object,
           erc);
      if (erc)
        ERROR("failed gpio_isr_handler_add() :%d", erc);
    }
    return gpio_config(&io_conf);
  }

  int deInit() { return 0; }

  int setMode(DigitalIn::Mode m) {
    _mode = m;
    return 0;
  }
  int onChange(PinChange pinChange, FunctionPointer fp, void *object) {
    _pinChange = pinChange;
    _fp = fp;
    _object = object;
    return 0;
  }
  PhysicalPin getPin() { return _gpio; }
};

DigitalIn &DigitalIn::create(PhysicalPin pin) {
  DigitalIn_ESP8266 *ptr = new DigitalIn_ESP8266(pin);
  return *ptr;
}
bool DigitalIn_ESP8266::_isrServiceInstalled = false;
/*
 ######                                                  #######
 #     #     #     ####      #     #####    ##    #      #     #  #    #   #####
 #     #     #    #    #     #       #     #  #   #      #     #  #    #     #
 #     #     #    #          #       #    #    #  #      #     #  #    #     #
 #     #     #    #  ###     #       #    ######  #      #     #  #    #     #
 #     #     #    #    #     #       #    #    #  #      #     #  #    #     #
 ######      #     ####      #       #    #    #  ###### #######   ####      #
 */
//================================================== DigitalOu =====
class DigitalOut_ESP8266 : public DigitalOut {
  PhysicalPin _gpio;
  Mode _mode = DOUT_NONE;

public:
  DigitalOut_ESP8266(uint32_t gpio) : _gpio(gpio) {}
  virtual ~DigitalOut_ESP8266() {}
  int setMode(DigitalOut::Mode m) {
    _mode = m;
    return 0;
  }
  int init() {
    //        INFO(" DigitalOut Init %d ", _gpio);
    esp_err_t erc = gpio_set_direction((gpio_num_t)_gpio, GPIO_MODE_OUTPUT);
    if (erc)
      ERROR("gpio_set_direction():%d", erc);
    gpio_config_t io_conf;
    // disable interrupt
    io_conf.intr_type = (gpio_int_type_t)GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (uint64_t)1 << _gpio;
    // disable pull-down mode
    io_conf.pull_down_en = (gpio_pulldown_t)((_mode == DOUT_PULL_DOWN) ? 1 : 0);
    // disable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)((_mode == DOUT_PULL_UP) ? 1 : 0);
    // configure GPIO with the given settings
    return gpio_config(&io_conf);
  }

  int deInit() { return 0; }

  int write(int x) { return gpio_set_level((gpio_num_t)_gpio, x ? 1 : 0); }

  PhysicalPin getPin() { return _gpio; }
};

DigitalOut &DigitalOut::create(PhysicalPin pin) {
  DigitalOut_ESP8266 *ptr = new DigitalOut_ESP8266(pin);
  return *ptr;
}
