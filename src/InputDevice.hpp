/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : InputDevice.hpp                 ************************************************************/
/********************************************* Description                      : Configuration ADS1256***********************************************************/
/********************************************* Date de création(fr)             : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      ************************************************************/

/* Ce fichier définit l'interface de la classe ADS1256 et regroupe l'ensemble des constantes de configuration 
   (Registres, Gains, Commandes) nécessaires au pilotage du convertisseur via le bus SPI.   */

/****************************************************************************************************************************************************************************/
#ifndef INPUTDEVICE_HPP                                                                          // dire au compilatuer si le ce fichier existe djéa passe à directement à endif            
#define INPUTDEVICE_HPP

#include <Arduino.h>
#include <SPI.h>
#include "ADS1256.hpp"


//const double MAX_VALUE_23_BIT_1 = 0x7FFFFF;                     // Valeur maximale d’un ADC 24 bits signé (2^23 - 1 = 8 388 607)
/************************************************************Creation d'une class propre à l'ADS****************************************************************/
class InputDevice {                                                                                   
private:
    uint8_t _csPin;
    uint8_t _drdyPin;
    SPIClass* _spi;
    uint32_t SPI_Speed;
    const double _vref;
    double _quantum;
    const double _Vexc;                                                                   // Tension d'excitation

public:

    InputDevice(SPIClass* spi, uint8_t cs, uint8_t drdy,double Vexc, uint32_t SPI_Speed = 100000);     // Creation d'un contructeur 
    void ReadInput();
    void createTask();
    void begin();  
    void sync();                                                                                   // FCT de synchronisation
    void select();                                                                                 // FCT por mettre ADS à l'écoute (LOW)
    void deselect();                                                                               // FCT pour mettre l'ADS  à l'état (HIGH) inactif
    int32_t readRaw();                                                                             // FCT pour lire la valeur Brute 
    double get_vref();                                                                             // tension de réference pour ADS
    double getQuantum();
    /*Mettre en privé*/
    void setChannel(uint8_t pos, uint8_t neg);                                                     
    void writeRegister(uint8_t reg, uint8_t value);                                                // FCT d'écriture dans un registre
    uint8_t getCsPin();
    
    
};



#endif