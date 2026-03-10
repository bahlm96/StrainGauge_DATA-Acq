#include <InputDevice.hpp>
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



ADS1256::ADS1256(SPIClass* spi, uint8_t cs, uint8_t drdy,double Vexc, uint32_t SPI_Speed) : // Le constructeur de la class ADS, cette fonction va être appelée au moment de la creation d'un objet dans le main.cpp
 _spi(spi), _csPin(cs), _drdyPin(drdy), _Vexc(Vexc) ,_vref(Vexc/2.0), SPI_Speed(SPI_Speed) 
 { }

void ADS1256::begin() {                          
    pinMode(_csPin, OUTPUT);                          // déclare le chip select commme sortie
    pinMode(_drdyPin, INPUT);                         // la broche DATAReady comme entrée 
    digitalWrite(_csPin, HIGH);                       // chip select Inactif
    writeRegister(REG_ADCON, 0x20);                   // Configuration initiale (le buffer et gain PGA à 1)
    sync();                                           // Après avoir changé de canal, il faut  synchroniser
    
}

void ADS1256::setChannel(uint8_t pos, uint8_t neg) {
    writeRegister(REG_MUX, (pos << 4) | neg);         // La commande pour le registre MUX est : (Entrée Positive << 4) | Entrée Négative
    sync();   
    delayMicroseconds(10);                            // Un court délai pour laisser le temps au multiplexeur de se stabiliser
}

 void ADS1256::writeRegister(uint8_t reg, uint8_t value) {
        select();
        _spi->transfer(CMD_WREG | reg);               // le CMD_WREG est la commande de base pour l'ecriture combinée avec l'adresse registre(reg)
        _spi->transfer(0x00);                         // Nombre de registres à écrire, le 0x00 on modifie une seul registre 
        _spi->transfer(value);                        // La donnée stocker dans le registre est envoyer
        delayMicroseconds(5);
        deselect();
    }

void ADS1256::sync() {                                // Une fonction pour faire synchroniser et réiveiller l'ADS
    select();
    _spi->transfer(CMD_SYNC);
    delayMicroseconds(5);
    _spi->transfer(CMD_WAKEUP);
    deselect();
}

int32_t ADS1256::readRaw() {                           // Lecture de la valeur brute 24 bits
    while (digitalRead(_drdyPin));                     // Attente que la donnée soit prête
    
    select();                                         // l'ADS est passé à LOW (donc Actif)
    _spi->transfer(CMD_RDATA);                        // communication vers l'ads par l'envoie de Ready DATA
    delayMicroseconds(10);                            // Temps de latence voir la datasheet ADS1256

    int32_t val = 0;
    val |= _spi->transfer(0xFF) << 16;               // Lire le première Octet (MSB)
    val |= _spi->transfer(0xFF) << 8;                // Lire le 2 Octets 
    val |= _spi->transfer(0xFF);                     // Lire l'octet de poids faible (LSB)
    
    deselect();                                      // Mettre l'ads à l'état HIGH (inactif)

    
    if (val & 0x800000) {                            // Extension de signe pour les valeurs négatives (24 bits -> 32 bits)
        val |= 0xFF000000;
    }
    
    return val;                                      // Retourne un entier signé sur 32 bits
}


void ADS1256::select() {                             // une fonction pour mettre l'ads à l'etat LOW
    digitalWrite(_csPin, LOW);
    
}

void ADS1256::deselect() {                           // une fonction pour mette l'ads à l'etat HIGH
    digitalWrite(_csPin, HIGH);
    
}



double ADS1256::get_vref(){                          
    // pour faire appel à Vref qu'est égale à 2.5V
    return _vref;

}
    