#include <InputDevice.hpp>
#include <ADS1256.hpp>
/***************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      **********************************************/
/********************************************* Fichier                          : InputDevice.cpp                 **********************************************/
/********************************************* Description                      : Implémentation du driver ADS1256 *********************************************/
/********************************************* Date de création                 : 03/03/2026                      **********************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      **********************************************/

/* Ce fichier contient les définitions des méthodes de la classe ADS1256. 
   Il gère la logique de communication SPI, la configuration des registres 
   et la récupération des données brutes de conversion 24 bits. */

/**************************************************************************************************************************************************************/

// Le constructeur de la class ADS, cette fonction va être appelée au moment de la creation d'un objet dans le main.cpp
InputDevice::InputDevice(SPIClass* spi, uint8_t cs, uint8_t drdy,double Vexc, uint32_t SPI_Speed, uint8_t sampling_period) : 
 _spi(spi), _csPin(cs), _drdyPin(drdy), _Vexc(Vexc), _vref(_Vexc/2), _quantum(_vref / (double) MAX_VALUE_23_BIT_1), SPI_Speed(SPI_Speed),_sampling_period(sampling_period) {
    pinMode(_csPin, OUTPUT);                          // déclare le chip select commme sortie
    pinMode(_drdyPin, INPUT);                         // la broche DATAReady comme entrée 
    digitalWrite(_csPin, HIGH);                       // chip select Inactif
 }

void InputDevice::begin() {    
    writeRegister(REG_ADCON,ADCON_RESET);             // Configuration initiale (le buffer et gain PGA à 1)
    setSamplingPeriod(this->_sampling_period);
    //sync();                                           // Après avoir changé de canal, il faut  synchroniser
}*

void InputDevice::setSamplingPeriod(uint8_t sampling_period){
    writeRegister(REG_DRATE,sampling_period);
}

void InputDevice::setChannel(uint8_t in1, uint8_t in2) {
    writeRegister(REG_MUX, ((in1 << 4) | in2));         // La commande pour le registre MUX est : (Entrée Positive << 4) | Entrée Négative
    sync();
}

void InputDevice::writeRegister(uint8_t reg, uint8_t value) {
        _spi->transfer(CMD_WREG | reg);               // le CMD_WREG est la commande de base pour l'ecriture combinée avec l'adresse registre(reg)
        _spi->transfer(REG_To_Write);                         // Nombre de registres à écrire, le 0x00 on modifie une seul registre 
        _spi->transfer(value);                        // La donnée stocker dans le registre est envoyer
        vTaskDelay(5 / portTICK_PERIOD_MS);
}

void InputDevice::sync() {                                // Une fonction pour faire synchroniser et réiveiller l'ADS
    _spi->transfer(CMD_SYNC);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    _spi->transfer(CMD_WAKEUP);
}

int32_t InputDevice::readRaw() {                           // Lecture de la valeur brute 24 bits
    while (digitalRead(_drdyPin));                     // Attente que la donnée soit prête                                      // l'ADS est passé à LOW (donc Actif)
    _spi->transfer(CMD_RDATA);                        // communication vers l'ads par l'envoie de Ready DATA
    vTaskDelay(10 / portTICK_PERIOD_MS);                // Temps de latence voir la datasheet ADS1256

    int32_t value = 0;
    value |= _spi->transfer(Masque_BITS) << Read_First_Octets;               
    value |= _spi->transfer(Masque_BITS) << Read_Second_Octets;                
    value |= _spi->transfer(Masque_BITS);                     // Lire l'octet de poids faible (LSB)
    
    if (value & Sign_Of_24_Bit) {                            // Extension de signe pour les valeurs négatives (24 bits -> 32 bits)
        value |= Extension_Sign_Bit;
    }
    
    return value;                                      // Retourne un entier signé sur 32 bits
}


void InputDevice::select() {                          // une fonction pour mettre l'ads à l'etat LOW
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