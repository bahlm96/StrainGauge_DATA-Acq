/**************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                       ********************************************/
/********************************************* Fichier                          : MemoryDevice.hpp                 ********************************************/
/********************************************* Description                      : Attributs et class CARTE mémoire SD *****************************************/
/********************************************* Date de création(fr)             : 03/03/2026                      *********************************************/
/********************************************* Date de dernière modification    : 05/06/2026                      *********************************************/

#ifndef MEMORYDEVICE_HPP
#define MEMORYDEVICE_HPP

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

class CardSD {
private:
    SPIClass* _spi;
    uint8_t   _csPin;
    String    _filename;
    uint8_t   _mask;    

public:
    CardSD(SPIClass* spi, uint8_t cs, String filename = "/measures.csv");

    bool begin(uint8_t mask = 0x0F);        // initialise la carte + écrit l'en-tête selon le masque
    void setMask(uint8_t mask);             // met à jour le masque (ferme le fichier actuel → nouvel en-tête)
    void saveRow(String dataCSVRow);
    void saveBuffer(double* buffer, int taille); // utilise _mask interne

private:
    void _writeHeader();                    // écrit l'en-tête CSV selon _mask
};

#endif
