/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : MemoryDevice.cpp                 ************************************************************/
/********************************************* Description                      : Fonction et class liée à la carte SD********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 17/04/2026                      ************************************************************/



/******Son rôle est de gérer le stockage des mesures sur une carte SD connectée à l'ESP32.
  Elle transforme les données de capteurs en un fichier CSV.*******************************************************************/



#include <MemoryDevice.hpp>

CardSD::CardSD(SPIClass* spi, uint8_t cs, String filename )                        // Creation d'un constructeur pour configurer la carte SD
        : _spi(spi), _csPin(cs), _filename(filename) {}       

bool CardSD::begin() {
    if (!SD.begin(_csPin, *_spi)) return false;
    
    
    if (SD.exists(_filename) == false || SD.open(_filename).size() == 0) {        // Si le fichier est vide ou n'existe pas
        File dataFile = SD.open(_filename, FILE_WRITE);                           // File_Write ouvrire le fichier en écriture 
        if (dataFile) {
            dataFile.println("TimeStamp,voltage1,voltage2,voltage3,Voltage4"); // écrire le nom des colonnes 
            dataFile.flush();
            dataFile.close();                                                     // fermer pour le sauvgarde sur la carte SD
        }
    }
    return true;
}
/*********************************FCT pour l'enregistrement des données sur la carte SD******************************************************/
void CardSD::saveRow(String dataCSVRow) {
    File dataFile = SD.open(_filename, FILE_APPEND);                              // File_Append :ajoute à la fin
    if (dataFile) {
        dataFile.println(dataCSVRow); 
        dataFile.flush();                                                        // On écrit la ligne de texte reçu
        dataFile.close();
    } else {
        Serial.println("Erreur : Impossible d'ouvrir " + _filename);             // s'il arrive pas à lire la carte SD, affiche donc ce message d'erreur 
    }
}

void CardSD::saveBuffer(double* buffer, int taille) {
    if (buffer == nullptr) return;
    File dataFile = SD.open(_filename, FILE_APPEND);
    if (dataFile) {
        for (int i = 0; i < taille; i += 4) {
            String line = "";
            line += String(millis()) + ",";                                      // timestamp 
            line += String(buffer[i], 4)     + ",";                              // Jauge 1
            line += String(buffer[i+1], 4)   + ",";                              // Jauge 2
            line += String(buffer[i+2], 4)   + ",";                              // Jauge 3
            line += String(buffer[i+3], 4);                                      // Jauge 4 
            
            dataFile.println(line);
        }
        dataFile.flush();
        dataFile.close(); 
    } else {
        Serial.println("Erreur SD : Impossible d'ouvrir " + _filename);
    }
}
