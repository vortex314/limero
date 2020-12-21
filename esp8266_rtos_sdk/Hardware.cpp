//#include <CircBuf.h>
#include <Hardware.h>
#include <Log.h>
#include <driver/gpio.h>
#include <driver/i2c.h>

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static struct ESP32 { i2c_port_t _i2c_port; } esp32 = {I2C_NUM_0};

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
  DigitalIn::Mode _mode = DIN_PULL_UP;
  void *_object = 0;
  FunctionPointer _fp = 0;
  PinChange _pinChange;
  static bool _isrServiceInstalled;

public:
  DigitalIn_ESP8266(PhysicalPin pin) : _gpio(pin), _pinChange(DIN_NONE){};

  virtual ~DigitalIn_ESP8266(){};

  static DigitalIn &create(PhysicalPin pin);

  int read() { return gpio_get_level((gpio_num_t)_gpio); }

  Erc init() {
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

  Erc deInit() { return 0; }

  Erc setMode(DigitalIn::Mode m) {
    _mode = m;
    return 0;
  }
  Erc onChange(PinChange pinChange, FunctionPointer fp, void *object) {
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
  Erc setMode(DigitalOut::Mode m) {
    _mode = m;
    return 0;
  }
  Erc init() {
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

  Erc deInit() { return 0; }

  Erc write(int x) { return gpio_set_level((gpio_num_t)_gpio, x ? 1 : 0); }

  PhysicalPin getPin() { return _gpio; }
};

DigitalOut &DigitalOut::create(PhysicalPin pin) {
  DigitalOut_ESP8266 *ptr = new DigitalOut_ESP8266(pin);
  return *ptr;
}

/**********************************************************************
 *
 *###    #####   #####
 #    #     # #     #
 #          # #
 #     #####  #
 #    #       #
 #    #       #     #
 ###   #######  #####

 *
 * *******************************************************************/
//================================================== I2C =======
/*
 * * */
//#define READ_BIT I2C_MASTER_READ /*!< I2C master read */
#define ACK_CHECK_EN 0x1  /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0 /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0       /*!< I2C ack value */
#define NACK_VAL 0x1      /*!< I2C nack value */

class I2C_ESP8266 : public I2C {
  std::string _txd;
  std::string _rxd;

  PhysicalPin _scl;
  PhysicalPin _sda;
  uint32_t _clock;
  uint8_t _slaveAddress;

  FunctionPointer _onTxd = 0;

  i2c_port_t _port;

public:
  I2C_ESP8266(PhysicalPin scl, PhysicalPin sda);
  ~I2C_ESP8266();
  Erc init();
  Erc deInit();

  Erc setClock(uint32_t clock) {
    _clock = clock;
    return 0;
  }
  Erc setSlaveAddress(uint8_t slaveAddress) {
    _slaveAddress = slaveAddress;
    return 0;
  }

  Erc write(uint8_t *data, uint32_t size);
  Erc read(uint8_t *data, uint32_t size);
  Erc write(uint8_t data);
};

I2C_ESP8266::I2C_ESP8266(PhysicalPin scl, PhysicalPin sda)
    :  _scl(scl), _sda(sda) {
  _port = esp32._i2c_port;
  _clock = 100000;
  _slaveAddress = 0x1E; // HMC 5883L
}

I2C_ESP8266::~I2C_ESP8266() {
  esp_err_t erc = i2c_driver_delete(_port);
  INFO(" erc : %d ", erc);
}

Erc I2C_ESP8266::init() {
  INFO(" I2C init : scl:%d ,sda :%d ", _scl, _sda);
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = (gpio_num_t)_sda;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_io_num = (gpio_num_t)_scl;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
 // conf.master.clk_speed = _clock;
  esp_err_t erc = i2c_param_config(_port, &conf);
  if (erc)
    WARN("i2c_param_config() : %d", erc);
  erc = i2c_driver_install(_port, conf.mode);
  if (erc)
    WARN("i2c_driver_install() : %d", erc);
  return erc;
}

Erc I2C_ESP8266::deInit() { return 0; }

Erc I2C_ESP8266::write(uint8_t *data, uint32_t size) {
  esp_err_t erc;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  erc = i2c_master_start(cmd);
  if (erc)
    WARN("i2c_master_start(cmd):%d", erc);
  erc = i2c_master_write_byte(cmd, (_slaveAddress << 1) | I2C_MASTER_WRITE,
                              ACK_CHECK_EN);
  if (erc)
    ERROR("i2c_master_write_byte():%d", erc);
  erc = i2c_master_write(cmd, data, size, ACK_CHECK_EN);
  if (erc)
    ERROR("i2c_master_write():%d", erc);
  erc = i2c_master_stop(cmd);
  if (erc)
    ERROR("i2c_master_stop():%d", erc);
  erc = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_RATE_MS);
  if (erc)
    ERROR("i2c_master_cmd_begin():%d", erc);
  i2c_cmd_link_delete(cmd);
  return erc;
}

Erc I2C_ESP8266::write(uint8_t b) { return write(&b, 1); }

Erc I2C_ESP8266::read(uint8_t *data, uint32_t size) {
  if (size == 0) {
    return 0;
  }
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (_slaveAddress << 1) | I2C_MASTER_READ,
                        ACK_CHECK_EN);
  if (size > 1) {
    i2c_master_read(cmd, data, size - 1, (i2c_ack_type_t)ACK_VAL);
  }
  i2c_master_read_byte(cmd, data + size - 1, (i2c_ack_type_t)NACK_VAL);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  return ret;
}

I2C &I2C::create(PhysicalPin scl, PhysicalPin sda) {
  I2C_ESP8266 *ptr = new I2C_ESP8266(scl, sda);
  return *ptr;
}

//========================================================   A D C
/*
 *
 *
    @    @@@@@@   @@@@@
   @ @   @     @ @     @
  @   @  @     @ @
 @     @ @     @ @
 @@@@@@@ @     @ @
 @     @ @     @ @     @
 @     @ @@@@@@   @@@@@

 *
 */
#include "driver/adc.h"

#define V_REF 1100
/*

class ADC_ESP8266 : public ADC {
  PhysicalPin _pin;
  uint32_t _channel;
  AdcUnit _unit;

public:
  ADC_ESP8266(PhysicalPin pin) {
    _pin = pin;
    _channel = (adc1_channel_t)UINT32_MAX;
    uint32_t channels = sizeof(AdcTable) / sizeof(struct AdcEntry);
    for (int i = 0; i < channels; i++) {
      if (AdcTable[i].pin == _pin) {
        _channel = AdcTable[i].channel;
        _unit = AdcTable[i].unit;
        return;
      }
    }
    ERROR("ADC channel not found for pin %d", pin);
  }
  Erc init() {
    //        check_efuse();
    INFO(" ADC init() pin %d ", _pin);
    if (_channel == (adc1_channel_t)UINT32_MAX)
      return EINVAL;
    esp_err_t erc;
    if (_unit == ADC1) {
      erc = adc1_config_width(ADC_WIDTH_BIT_10);
      if (erc)
        ERROR("adc1_config_width(): %d", erc);
      erc =
          adc1_config_channel_atten((adc1_channel_t)_channel, ADC_ATTEN_DB_11);
      if (erc)
        ERROR("adc1_config_channel_atten():%d", erc);
                esp_adc_cal_get_characteristics(V_REF, ADC_ATTEN_DB_11,
       ADC_WIDTH_BIT_12,
       &_characteristics);
    } else if (_unit == ADC2) {
      //    erc = adc2_config_channel_atten((adc2_channel_t)_channel,
      //    ADC_ATTEN_DB_11);
      erc = adc2_config_channel_atten(ADC2_CHANNEL_3, ADC_ATTEN_DB_11);
      if (erc)
        ERROR("adc2_config_channel_atten():%d:%s", erc, esp_err_to_name(erc));
    }

    return 0;
  }

  int getValue() {
    if (_unit == ADC1) {
      uint32_t voltage = adc1_get_raw((adc1_channel_t)_channel);
      //   uint32_t voltage = adc1_to_voltage((adc1_channel_t)_channel,
      //   &_characteristics);
      return voltage;
    } else if (_unit == ADC2) {
      int voltage;
      esp_err_t r =
          adc2_get_raw((adc2_channel_t)_channel, ADC_WIDTH_10Bit, &voltage);
      if (r == ESP_OK) {
        return voltage;
      } else {
        ERROR(" adc2_get_raw() failed : %d ", r);
        return -1;
      }
    } else {
      ERROR(" invalid ADC unit %d ", _unit);
      return -2;
    }
  }
};

ADC &ADC::create(PhysicalPin pin) {
  ADC_ESP8266 *ptr = new ADC_ESP8266(pin);
  return *ptr;
}
*/
/*******************************************************************************

 #####  ######    ###
 #    # #     #    #
 #      #     #    #
 #####  ######     #
      # #          #
#     # #          #
 #####  #         ###

 *****************************************************************************/

#include "driver/spi.h"
#include "esp_system.h"

class SPI_ESP8266 : public Spi {
protected:
  FunctionPointer _onExchange;
  uint32_t _clock;
  uint32_t _mode;
  bool _lsbFirst;
  PhysicalPin _miso, _mosi, _sck, _cs;
  void *_object = 0;
  spi_host_t  _spi;

public:
  SPI_ESP8266(PhysicalPin miso, PhysicalPin mosi, PhysicalPin sck, PhysicalPin cs)
      : _miso(12), _mosi(13), _sck(14), _cs(15) { // don't care here
    _clock = 100000;
    _mode = 0;
    _lsbFirst = true;
    _spi = HSPI_HOST;
    _onExchange = 0;
  }

  Erc init() {
    INFO(" SPI_ESP8266 : miso : %d, mosi : %d , sck : %d , cs : %d ", _miso,
         _mosi, _sck, _cs);

/*
    spi_bus_config_t buscfg;
    bzero(&buscfg, sizeof(buscfg));
    buscfg.miso_io_num = _miso;
    buscfg.mosi_io_num = _mosi;
    buscfg.sclk_io_num = _sck;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 0;

    // Initialize the SPI bus
    ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    if (ret) {
      ERROR("spi_bus_initialize(HSPI_HOST, &buscfg, 1) = %d ", ret);
      return EIO;
    }*/

	    spi_config_t spi_config;
    // Load default interface parameters
    // CS_EN:1, MISO_EN:1, MOSI_EN:1, BYTE_TX_ORDER:1, BYTE_TX_ORDER:1, BIT_RX_ORDER:0, BIT_TX_ORDER:0, CPHA:0, CPOL:0
    spi_config.interface.val = SPI_DEFAULT_INTERFACE;

    // Load default interrupt enable
    // TRANS_DONE: true, WRITE_STATUS: false, READ_STATUS: false, WRITE_BUFFER: false, READ_BUFFER: false
    spi_config.intr_enable.val = SPI_MASTER_DEFAULT_INTR_ENABLE;    
    spi_config.mode = SPI_MASTER_MODE; // Set SPI to master mode , ESP8266 Only support half-duplex
    spi_config.clk_div = SPI_20MHz_DIV;     // Set the SPI clock frequency division factor
    spi_config.event_cb = NULL; // Register SPI event callback function
    spi_init(HSPI_HOST, &spi_config);

    return 0;
  };

  Erc deInit() {
    esp_err_t ret = spi_deinit(_spi);
    if (ret) {
      ERROR("spi_bus_remove_device(_spi) = %d ", ret);
      return EIO;
    }
    return 0;
  }

  Erc exchange(std::string &in, std::string &out) {

	      spi_trans_t trans;
	uint32_t len =  out.length();
	uint32_t inData[16],outData[16];
	memcpy(outData,out.data(),out.length());

	BZERO(trans);

    if (len > 16) {
        WARN("ESP8266 only support transmit 64bytes(16 * sizeof(uint32_t)) one time");
        return EINVAL;
    }

    memset(&trans, 0x0, sizeof(trans));
    trans.bits.val = 0;            // clear all bit

	trans.mosi = outData;
    trans.miso = inData;
	trans.bits.mosi =  out.length()*8;
	trans.bits.miso = in.length()*8;
    trans.bits.cmd = 0;
    trans.bits.addr = 0;     
    trans.cmd = NULL;
    trans.addr = NULL;

	spi_trans(HSPI_HOST, &trans); 

    return 0; // Should have had no issues.
  };

  Erc setClock(uint32_t clock) {
    _clock = clock;
    return 0;
  }

  Erc setMode(SpiMode mode) {
    _mode = mode;
    return 0;
  }

  Erc setLsbFirst(bool f) {
    _lsbFirst = f;
    return true;
  }

  Erc onExchange(FunctionPointer p, void *ptr) { return 0; }

  Erc setHwSelect(bool b) { return 0; }
};

Spi::~Spi() {}

Spi &Spi::create(PhysicalPin miso, PhysicalPin mosi, PhysicalPin sck,
                 PhysicalPin cs) {
  SPI_ESP8266 *ptr = new SPI_ESP8266(miso, mosi, sck, cs);
  return *ptr;
}

/*
 @     @    @    @@@@@@  @@@@@@@
 @     @   @ @   @     @    @
 @     @  @   @  @     @    @
 @     @ @     @ @@@@@@     @
 @     @ @@@@@@@ @   @      @
 @     @ @     @ @    @     @
  @@@@@  @     @ @     @    @

 */
/*
#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024

#define TAG "uart0"
#define PATTERN_CHR_NUM 1

class UART_ESP8266 : public UART {
  FunctionPointer _onRxd;
  FunctionPointer _onTxd;
  void *_onRxdVoid = 0;
  void *_onTxdVoid = 0;
  uint32_t clock = 9600;
  uart_port_t _uartNum;
  uint32_t _pinTxd;
  uint32_t _pinRxd;
  uint32_t _baudrate;
  QueueHandle_t _queue = 0;
  CircBuf _rxdBuf;
  uint32_t _driver;
  uart_config_t uart_config;
  TaskHandle_t _taskHandle;

public:
  UART_ESP8266(uint32_t driver, PhysicalPin txd, PhysicalPin rxd)
      : _pinTxd(txd), _pinRxd(rxd), _rxdBuf(300) {
    _driver = driver;
    switch (driver) {
    case 0: {
      _uartNum = UART_NUM_0;
      break;
    }
    case 1: {
      _uartNum = UART_NUM_1;
      break;
    }
    }
    _baudrate = 9600;
    _onTxd = 0;
    _onRxd = 0;
    ZERO(uart_config);
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    _taskHandle = 0;
  }

  virtual ~UART_ESP8266() {}

  Erc mode(const char *m) {
    if (m[0] == '8')
      uart_config.data_bits = UART_DATA_8_BITS;
    else if (m[0] == '7')
      uart_config.data_bits = UART_DATA_7_BITS;
    else if (m[0] == '6')
      uart_config.data_bits = UART_DATA_6_BITS;
    else if (m[0] == '5')
      uart_config.data_bits = UART_DATA_5_BITS;
    else
      return EINVAL;
    if (m[1] == 'E')
      uart_config.parity = UART_PARITY_EVEN;
    else if (m[1] == 'O')
      uart_config.parity = UART_PARITY_ODD;
    else if (m[1] == 'N')
      uart_config.parity = UART_PARITY_DISABLE;
    else
      return EINVAL;
    if (m[2] == '1')
      uart_config.stop_bits = UART_STOP_BITS_1;
    else if (m[2] == '2')
      uart_config.stop_bits = UART_STOP_BITS_2;
    else
      return EINVAL;
    return 0;
  }

  Erc init() {
    uart_config.baud_rate = _baudrate;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    //       uart_config.use_ref_tick = false;
    uart_config.rx_flow_ctrl_thresh = 1;

    int rc = uart_param_config(_uartNum, &uart_config);
    if (rc)
      ERROR(" uart_param_config() failed : %d  ", rc);

    if (_uartNum == UART_NUM_0) {
      uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                   UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); // no CTS,RTS
    } else {
      uart_set_pin(_uartNum, _pinTxd, _pinRxd, UART_PIN_NO_CHANGE,
                   UART_PIN_NO_CHANGE); // no CTS,RTS
    }

    if (uart_driver_install(_uartNum, RX_BUF_SIZE, 0, 20, &_queue, 0))
      ERROR("uart_driver_install() failed.");

    std::string taskName;
    string_format(taskName, "uart_event_task_%d", _driver);
    xTaskCreate(uart_event_task, taskName.c_str(), 3120, this,
                tskIDLE_PRIORITY + 5, &_taskHandle);
    return 0;
  }

  Erc deInit() {
    int rc = uart_driver_delete(_uartNum);
    vTaskDelete(_taskHandle);
    return rc == ESP_OK ? 0 : EIO;
  }

  Erc setClock(uint32_t clock) {
    _baudrate = clock;
    return 0;
  }

  Erc write(const uint8_t *data, uint32_t length) {
    if (uart_write_bytes(_uartNum, (const char *)data, length) == length)
      return 0;
    return EIO;
  }

  Erc write(uint8_t b) {
    if (uart_write_bytes(_uartNum, (const char *)&b, 1) == 1)
      return 0;
    return 0;
  }

  Erc read(std::string &bytes) {
    while (_rxdBuf.hasData() && bytes.hasSpace(1))
      bytes.write(_rxdBuf.read());
    return 0;
  }

  uint8_t read() { return _rxdBuf.read(); }

  void onRxd(FunctionPointer fr, void *pv) {
    _onRxd = fr;
    _onRxdVoid = pv;
    return;
  }

  void onTxd(FunctionPointer fw, void *pv) {
    _onTxd = fw;
    _onTxdVoid = pv;
    return;
  }

  uint32_t hasSpace() { return 0; }

  uint32_t hasData() { return _rxdBuf.hasData(); }

  static void uart_event_task(void *pvParameters) {
    UART_ESP8266 *uartEsp32 = (UART_ESP8266 *)pvParameters;
    uartEsp32->event_task();
  }
  void event_task() {
    uart_event_t event;
    size_t buffered_size;
    uint8_t *dtmp = (uint8_t *)malloc(RX_BUF_SIZE);
    INFO(" uart%d task started ", _uartNum);
    for (;;) {
      // Waiting for UART event.
      if (xQueueReceive(_queue, (void *)&event, (portTickType)portMAX_DELAY)) {
        bzero(dtmp, RX_BUF_SIZE);
        switch (event.type) {
        // Event of UART receving data

        case UART_DATA: {
          int n = uart_read_bytes(_uartNum, dtmp, event.size, portMAX_DELAY);
          if (n < 0)
            ERROR("uart_read_bytes() failed.");
          for (uint32_t i = 0; i < event.size; i++) {
            _rxdBuf.write(dtmp[i]);
            //						INFO(" RXD
            // 0x%X", dtmp[i]);
          }
          if (_onRxd)
            _onRxd(_onRxdVoid);
          break;
        }
        // Event of HW FIFO overflow detected
        case UART_FIFO_OVF:
          INFO("hw fifo overflow");
          // If fifo overflow happened, you should consider adding
          // flow control for your application. The ISR has already
          // reset the rx FIFO, As an example, we directly flush the
          // rx buffer here in order to read more data.
          uart_flush_input(_uartNum);
          xQueueReset(_queue);
          break;
        // Event of UART ring buffer full
        case UART_BUFFER_FULL:
          INFO("ring buffer full");
          // If buffer full happened, you should consider encreasing
          // your buffer size As an example, we directly flush the rx
          // buffer here in order to read more data.
          uart_flush_input(_uartNum);
          xQueueReset(_queue);
          break;
        // Event of UART RX break detected
        case UART_BREAK:
          INFO("uart rx break");
          break;
        // Event of UART parity check error
        case UART_PARITY_ERR:
          WARN("uart parity error");
          break;
        // Event of UART frame error
        case UART_FRAME_ERR:
          WARN("uart frame error");
          break;
        // UART_PATTERN_DET
        case UART_PATTERN_DET: {
          uart_get_buffered_data_len(_uartNum, &buffered_size);
          int pos = uart_pattern_pop_pos(_uartNum);
          INFO("[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos,
               buffered_size);
          if (pos == -1) {
            // There used to be a UART_PATTERN_DET event, but the
            // pattern position queue is full so that it can not
            // record the position. We should set a larger queue
            // size. As an example, we directly flush the rx buffer
            // here.
            uart_flush_input(_uartNum);
          } else {
            int n =
                uart_read_bytes(_uartNum, dtmp, pos, 100 / portTICK_PERIOD_MS);
            if (n < 0)
              ERROR("uart_read_bytes() failed.");

            uint8_t pat[PATTERN_CHR_NUM + 1];
            memset(pat, 0, sizeof(pat));
            n = uart_read_bytes(_uartNum, pat, PATTERN_CHR_NUM,
                                100 / portTICK_PERIOD_MS);
            if (n < 0)
              ERROR("uart_read_bytes() failed.");
            INFO("read data: %s", dtmp);
            INFO("read pat : %s", pat);
          }
          break;
        }
        // Others
        default:
          INFO("uart event type: %d", event.type);
          break;
        }
      }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
  }
};

UART &UART::create(uint32_t module, PhysicalPin txd, PhysicalPin rxd) {
  UART_ESP8266 *ptr = new UART_ESP8266(module, txd, rxd);
  return *ptr;
}
*/
/*
 #####
 #     #   ####   #    #  #    #  ######   ####    #####   ####   #####
 #        #    #  ##   #  ##   #  #       #    #     #    #    #  #    #
 #        #    #  # #  #  # #  #  #####   #          #    #    #  #    #
 #        #    #  #  # #  #  # #  #       #          #    #    #  #####
 #     #  #    #  #   ##  #   ##  #       #    #     #    #    #  #   #
 #####    ####   #    #  #    #  ######   ####      #     ####   #    #

 */
Connector::Connector(uint32_t idx) // defined by PCB layout
{
  /* OLD PCB
   if (idx == 1) {
   _physicalPins[LP_TXD] = 17;
   _physicalPins[LP_RXD] = 16;
   _physicalPins[LP_SCL] = 15;
   _physicalPins[LP_SDA] = 4;
   _physicalPins[LP_MISO] = 19;
   _physicalPins[LP_MOSI] = 23;
   _physicalPins[LP_SCK] = 18;
   _physicalPins[LP_CS] = 21;
   } else if (idx == 2) {
   _physicalPins[LP_TXD] = 32;
   _physicalPins[LP_RXD] = 33;
   _physicalPins[LP_SCL] = 25;
   _physicalPins[LP_SDA] = 26;
   _physicalPins[LP_MISO] = 27;
   _physicalPins[LP_MOSI] = 14;
   _physicalPins[LP_SCK] = 13;
   _physicalPins[LP_CS] = 12;
   } else if (idx == 3) {
   _physicalPins[LP_TXD] = 1;
   _physicalPins[LP_RXD] = 3;
   _physicalPins[LP_SCL] = 22;
   _physicalPins[LP_SDA] = 5;
   _physicalPins[LP_MISO] = 36;
   _physicalPins[LP_MOSI] = 39;
   _physicalPins[LP_SCK] = 34;
   _physicalPins[LP_CS] = 35;
   }*/
  if (idx == 1) {
    _physicalPins[LP_TXD] = 19;
    _physicalPins[LP_RXD] = 36;
    _physicalPins[LP_SCL] = 25;
    _physicalPins[LP_SDA] = 26;
    _physicalPins[LP_MISO] = 34;
    _physicalPins[LP_MOSI] = 23;
    _physicalPins[LP_SCK] = 17;
    _physicalPins[LP_CS] = 32;
  } else if (idx == 2) {
    _physicalPins[LP_TXD] = 18;
    _physicalPins[LP_RXD] = 39;
    _physicalPins[LP_SCL] = 27;
    _physicalPins[LP_SDA] = 14;
    _physicalPins[LP_MISO] = 35;
    _physicalPins[LP_MOSI] = 22;
    _physicalPins[LP_SCK] = 16;
    _physicalPins[LP_CS] = 33;
  }

  _spi = 0;
  _i2c = 0;
  _uart = 0;
  _pinsUsed = 0;
  _connectorIdx = idx;
  _adc = 0;
}

const char *sLogicalPin[] = {"TXD",  "RXD",  "SCL", "SDA",
                             "MISO", "MOSI", "SCK", "CS"};

PhysicalPin Connector::toPin(uint32_t logicalPin) {
  DEBUG(" UEXT%d %s[%d] => GPIO_%d", _connectorIdx, sLogicalPin[logicalPin],
        logicalPin, _physicalPins[logicalPin]);
  return _physicalPins[logicalPin];
}

const char *Connector::uextPin(uint32_t logicalPin) {
  return sLogicalPin[logicalPin];
}
/*
UART &Connector::getUART() {
  lockPin(LP_TXD);
  lockPin(LP_RXD);
  _uart = new UART_ESP8266(_connectorIdx, toPin(LP_TXD), toPin(LP_RXD));
  return *_uart;
}*/

Spi &Connector::getSPI() {
  _spi = new SPI_ESP8266(toPin(LP_MISO), toPin(LP_MOSI), toPin(LP_SCK),
                       toPin(LP_CS));
  lockPin(LP_MISO);
  lockPin(LP_MOSI);
  lockPin(LP_SCK);
  lockPin(LP_CS);
  return *_spi;
}
I2C &Connector::getI2C() {
  lockPin(LP_SDA);
  lockPin(LP_SCL);
  _i2c = new I2C_ESP8266(toPin(LP_SCL), toPin(LP_SDA));
  return *_i2c;
}
/*
ADC &Connector::getADC(LogicalPin pin) {
  lockPin(LP_SDA);
  ADC *adc = new ADC_ESP8266(toPin(pin));
  return *adc;
}*/

DigitalOut &Connector::getDigitalOut(LogicalPin lp) {
  lockPin(lp);
  DigitalOut *_out = new DigitalOut_ESP8266(toPin(lp));
  return *_out;
}

DigitalIn &Connector::getDigitalIn(LogicalPin lp) {
  lockPin(lp);
  DigitalIn *_in = new DigitalIn_ESP8266(toPin(lp));
  return *_in;
}

void Connector::lockPin(LogicalPin lp) {
  if (_pinsUsed & (1 << lp)) {
    ERROR(" PIN in use %d : %s  >>>>>>>>>>>>>>>>>> %X", lp, sLogicalPin[lp],
          _pinsUsed);
  } else {
    DEBUG(" PIN locked : %d :%s ,%X", lp, sLogicalPin[lp], _pinsUsed);
    _pinsUsed |= (1 << lp);
  }
}

/*




 @@@    @@@@@   @@@@@
 @    @     @ @     @
 @          @ @
 @     @@@@@   @@@@@
 @    @             @
 @    @       @     @
 @@@   @@@@@@@  @@@@@

 @    @@@@@@   @@@@@
 @ @   @     @ @     @
 @   @  @     @ @
 @     @ @     @ @
 @@@@@@@ @     @ @
 @     @ @     @ @     @
 @     @ @@@@@@   @@@@@



 */
