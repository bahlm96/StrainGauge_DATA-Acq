#include <InputDevice.hpp>
#include <ADS1256.hpp>
/***************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      **********************************************/
/********************************************* Fichier                          : InputDevice.cpp                 **********************************************/
/********************************************* Description                      : Implémentation du driver ADS1256 *********************************************/
/********************************************* Date de création                 : 03/03/2026                      **********************************************/
/********************************************* Date de dernière modification    : 18/06/2026                      **********************************************/

/* Changelog :
    v0.1.1  --> readRaw() : timeout 5 ms sur attente DRDY via micros()
                Evite le blocage infini qui génère une valeur parasite (pic)
*/

/**************************************************************************************************************************************************************/

InputDevice::InputDevice(SPIClass* spi, uint8_t cs, uint8_t drdy, double Vexc, uint32_t SPI_Speed, uint8_t sampling_period, uint8_t gain) :
    _spi(spi), _csPin(cs), _drdyPin(drdy), _Vexc(Vexc), _vref(_Vexc/2), _quantum(0.0),
    SPI_Speed(SPI_Speed), _sampling_period(sampling_period), _gain(gain) {
    pinMode(_csPin,   OUTPUT);
    pinMode(_drdyPin, INPUT);
    digitalWrite(_csPin, HIGH);
    _quantum = (2.0 * _vref) / ((double)MAX_VALUE_23_BIT_1);
}

void InputDevice::begin() {
    _spi->beginTransaction(SPISettings(SPI_Speed, MSBFIRST, SPI_MODE1));
    select();
    writeRegister(REG_ADCON, _gain);
    writeRegister(REG_DRATE, this->_sampling_period);
    writeRegister(REG_MUX,   ADS1256_MUX_DIFF(0, 1));
    deselect();
    _spi->endTransaction();
}

void InputDevice::setSamplingPeriod(uint8_t sampling_period) {
    writeRegister(REG_DRATE, sampling_period);
}

void InputDevice::setChannel(uint8_t in1, uint8_t in2) {
    writeRegister(REG_MUX, ((in1 << 4) | in2));
    syncAndWakeup();
    uint32_t t = millis();
    while (digitalRead(_drdyPin) == HIGH) {
        if ((millis() - t) > 200) break;
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

void InputDevice::writeRegister(uint8_t reg, uint8_t value) {
    select();
    _spi->transfer(CMD_WREG | reg);
    _spi->transfer(REG_To_Write);
    _spi->transfer(value);
    deselect();
}

void InputDevice::syncAndWakeup() {
    select();
    _spi->transfer(CMD_SYNC);
    delayMicroseconds(5);
    _spi->transfer(CMD_WAKEUP);
    delayMicroseconds(5);
    deselect();
}

int32_t InputDevice::readRaw() {
    /* Timeout 5 ms : si DRDY ne descend pas (glitch WiFi/interruption),
       on retourne 0 au lieu de bloquer indéfiniment → pas de valeur parasite */
    uint32_t t = micros();
    while (digitalRead(_drdyPin) == HIGH) {
        if ((micros() - t) > 5000) return 0;
    }

    select();
    _spi->transfer(CMD_RDATA);
    delayMicroseconds(10);

    int32_t value = 0;
    value |= (int32_t)_spi->transfer(Masque_BITS) << Read_First_Octets;
    value |= (int32_t)_spi->transfer(Masque_BITS) << Read_Second_Octets;
    value |= (int32_t)_spi->transfer(Masque_BITS);

    deselect();

    if (value & Sign_Of_24_Bit) value |= Extension_Sign_Bit;
    return value;
}

void InputDevice::Set_ADS1256_SPS(uint8_t drate) {
    writeRegister(REG_DRATE, drate);
    this->syncAndWakeup();
}

uint8_t InputDevice::get_ADS1256_SPS(uint8_t drate) {
    _CurrentDRATEADS1256 = drate;
    writeRegister(REG_DRATE, drate);
    this->syncAndWakeup();
    return _CurrentDRATEADS1256;
}

void InputDevice::select()   { digitalWrite(_csPin, LOW);  }
void InputDevice::deselect() { digitalWrite(_csPin, HIGH); }

double  InputDevice::getQuantum() { return _quantum; }
double  InputDevice::get_vref()   { return _vref;    }
uint8_t InputDevice::getCsPin()   { return _csPin;   }

void InputDevice::createTask() {}

void InputDevice::beginTransaction() {
    _spi->beginTransaction(SPISettings(SPI_Speed, MSBFIRST, SPI_MODE1));
}

void InputDevice::endTransaction() {
    _spi->endTransaction();
}