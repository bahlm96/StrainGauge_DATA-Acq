/**************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                       ********************************************/
/********************************************* Fichier                          : MemoryDevice.hpp                 ********************************************/
/********************************************* Description                      : Attributs et class CARTE mémoire SD *****************************************/
/********************************************* Date de création(fr)             : 03/03/2026                      *********************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      *********************************************/

/**************************************************************************************************************************************************************/




#ifndef MEMORYDEVICE_HPP
#define MEMORYDEVICE_HPP

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#ifndef BUFFER_TAILLE
#define BUFFER_TAILLE 2048
#endif
/*Crée une class pour la carte mémoire SD physique avec ces paramètres : bus spi, son chip select & le nome du fichier*/
class CardSD {
private:
    
    SPIClass* _spi; 
    uint8_t _csPin;    
    String _filename;
    
public:
    CardSD(SPIClass* spi, uint8_t cs, String filename = "/measures.csv");         // creation d'un constructeur 
    bool begin();                                                                 // FCT d'initialisation de la carte 
    void saveRow(String dataCSVRow) ;                                             // FCT pour enregistrement des données 
    void saveBuffer(double* buffer, int taille);
private:
    
};

#endif

