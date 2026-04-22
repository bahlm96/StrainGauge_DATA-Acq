#include <InputDevice.hpp>
#include <ADS1256.hpp>
/***************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      **********************************************/
/********************************************* Fichier                          : InputDevice.cpp                 **********************************************/
/********************************************* Description                      : Implémentation du driver ADS1256 *********************************************/
/********************************************* Date de création                 : 03/03/2026                      **********************************************/
/********************************************* Date de dernière modification    : 21/04/2026                      **********************************************/



/**************************************************************************************************************************************************************/

// Le constructeur de la class ADS, cette fonction va être appelée au moment de la creation d'un objet dans le main.cpp
InputDevice::InputDevice(SPIClass* spi, uint8_t cs, uint8_t drdy,double Vexc, uint32_t SPI_Speed, uint8_t sampling_period, uint8_t gain) : 
    _spi(spi), _csPin(cs), _drdyPin(drdy), _Vexc(Vexc), _vref(_Vexc/2), _quantum(0.0), SPI_Speed(SPI_Speed),_sampling_period(sampling_period),_gain(gain) {
    pinMode(_csPin, OUTPUT);                          // déclare le chip select commme sortie
    pinMode(_drdyPin, INPUT);                         // la broche DATAReady comme entrée 
    digitalWrite(_csPin, HIGH);                       // chip select Inactif
    _quantum =(2.0 * _vref) / ((double) MAX_VALUE_23_BIT_1);
    //_quantum =(2.0 * _vref) / (8388607);
 }

void InputDevice::begin() {    
    //writeRegister(REG_ADCON,ADCON_RESET);             // ne marche pas 
    _spi->beginTransaction(SPISettings(SPI_Speed, MSBFIRST, SPI_MODE1));
    select();
    writeRegister(REG_ADCON, _gain);   // gain = 1
    writeRegister(REG_DRATE, this->_sampling_period);
    //writeRegister(REG_MUX, ADS1256_MUX_AIN0);   //  AIN0+ / AIN0-
    writeRegister(REG_MUX, ADS1256_MUX_DIFF(0, 1));  // AIN0+ / AIN1-
    
    deselect();
    _spi->endTransaction();
}

void InputDevice::setSamplingPeriod(uint8_t sampling_period){
    //select();
    writeRegister(REG_DRATE,sampling_period);
}

void InputDevice::setChannel(uint8_t in1, uint8_t in2) {
    writeRegister(REG_MUX, ((in1 << 4) | in2));         // La commande pour le registre MUX est : (Entrée Positive << 4) | Entrée Négative
    syncAndWakeup();
    


}

void InputDevice::writeRegister(uint8_t reg, uint8_t value) {
        select();
        _spi->transfer(CMD_WREG | reg);               // le CMD_WREG est la commande de base pour l'ecriture combinée avec l'adresse registre(reg)
        _spi->transfer(REG_To_Write);                         // Nombre de registres à écrire, le 0x00 on modifie une seul registre 
        _spi->transfer(value);                        // La donnée stocker dans le registre est envoyer
        deselect();
}

void InputDevice::syncAndWakeup() {                                // Une fonction pour faire synchroniser et réiveiller l'ADS
    
  select();
  _spi->transfer(CMD_SYNC);
  delayMicroseconds(5);
  //vTaskDelay(5/portTICK_PERIOD_MS);
  _spi->transfer(CMD_WAKEUP);
  //vTaskDelay(5/portTICK_PERIOD_MS);
  delayMicroseconds(5);
  deselect();
    
}

int32_t InputDevice::readRaw() {                                   // Lecture de la valeur brute 24 bits
    
    
    while (digitalRead(_drdyPin) == HIGH);
    select();
    _spi->transfer(CMD_RDATA);
    delayMicroseconds(10);

    int32_t value = 0;
    value |= (int32_t)_spi->transfer(Masque_BITS) << Read_First_Octets;               
    value |= (int32_t)_spi->transfer(Masque_BITS) << Read_Second_Octets;                
    value |= (int32_t)_spi->transfer(Masque_BITS);                     // Lire l'octet de poids faible (LSB)
    
    deselect();
    if (value & Sign_Of_24_Bit) {                                   // Extension de signe pour les valeurs négatives (24 bits -> 32 bits)
        value |= Extension_Sign_Bit;
    }

   // while (digitalRead(_drdyPin) != HIGH);                       // Attente que la donnée soi    
    return value;                                                  // Retourne un entier signé sur 32 bits
    
}

void InputDevice::Set_ADS1256_SPS(uint8_t drate){
    writeRegister(REG_DRATE, drate);
    this->syncAndWakeup();
}

uint8_t InputDevice::get_ADS1256_SPS(uint8_t drate) {
    _CurrentDRATEADS1256 = drate;
    writeRegister(REG_DRATE, drate);
    this->syncAndWakeup();
    return _CurrentDRATEADS1256;
}



void InputDevice::select() {                                      // une fonction pour mettre l'ads à l'etat LOW
    digitalWrite(_csPin, LOW);    
}

void InputDevice::deselect() {
    digitalWrite(_csPin, HIGH);    
}

double InputDevice::getQuantum() {
    return _quantum;
}

double InputDevice::get_vref(){
    return _vref;
}

uint8_t InputDevice::getCsPin(){
    return _csPin;
}

void InputDevice::createTask(){
    //xTaskCreate(Task_ON,"trat_task",2048,NULL,1,NULL);
}


void InputDevice::beginTransaction() {
    select();
    _spi->beginTransaction(SPISettings(SPI_Speed, MSBFIRST, SPI_MODE1));
}

void InputDevice::endTransaction() {
    _spi->endTransaction();
    deselect();
}