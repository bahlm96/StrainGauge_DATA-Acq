/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : MemoryDevice.cpp                 ************************************************************/
/********************************************* Description                      : Fonction et class liée à la carte SD********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      ************************************************************/

/* Ce fichier définit l'interface de la classe ADS1256 et regroupe l'ensemble des constantes de configuration 
   (Registres, Gains, Commandes) nécessaires au pilotage du convertisseur via le bus SPI. 
   Il sert de pont entre le matériel (DataSheet TI) et la logique logicielle du gestionnaire. */

/******Son rôle est de gérer le stockage des mesures sur une carte SD connectée à l'ESP32.
  Elle transforme les données de capteurs en un fichier CSV.*******************************************************************/



#include <MemoryDevice.hpp>

CardSD::CardSD(SPIClass* spi, uint8_t cs, String filename )                      // Creation d'un constructeur pour configurer la carte SD
        : _spi(spi), _csPin(cs), _filename(filename) {}       

bool CardSD::begin() {
    if (!SD.begin(_csPin, *_spi)) return false;
    
    
    if (SD.exists(_filename) == false || SD.open(_filename).size() == 0) {        // Si le fichier est vide ou n'existe pas
        File dataFile = SD.open(_filename, FILE_WRITE);                           // File_Write ouvrire le fichier en écriture 
        if (dataFile) {
            dataFile.println("Time,voltage1,voltage2,deformation1,deformation2"); // écrire le nom des colonnes 
            dataFile.close();                                                     // fermer pour le sauvgarde sur la carte SD
        }
    }
    return true;
}
/*********************************FCT pour l'enregistrement des données sur la carte SD******************************************************/
void CardSD::saveRow(String dataCSVRow) {
    File dataFile = SD.open(_filename, FILE_APPEND);                              // File_Append :ajoute à la fin
    if (dataFile) {
        dataFile.println(dataCSVRow);                                             // On écrit la ligne de texte reçu
        dataFile.close();
    } else {
        Serial.println("Erreur : Impossible d'ouvrir " + _filename);              // s'il arrive pas à lire la carte SD, affiche donc ce message d'erreur 
    }
}
