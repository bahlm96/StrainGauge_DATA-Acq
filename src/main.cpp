/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : Main.cpp                        ************************************************************/
/********************************************* Description                      : Creation d'objets et initialisation***********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      ************************************************************/

/*********déclarer les objets, de configurer les bus SPI et de mettre en relation les capteurs avec le gestionnaire de tâches qu'est InputReaderManager*********/

#include <Arduino.h>
#include <main.hpp>

#define speed_monitor  115200                                                       // le port série pour le débug
const char *filename = "/cardSD.csv";                                               // créee un fichier en format (.csv)

/*Initialisation des deux BUS SPI de l'ESP32*/
SPIClass vspi(VSPI);
SPIClass hspi(HSPI);

InputReaderManager inputReaderManager(1000);                                        // periode d'echantillonage 1 seconde

CardSD myCard(&hspi, SD_PIN, "/measures.csv");                                      // creation de l'objet mycard

ADS1256 ads1(&vspi, ADS_1_CS_PIN, ADS_1_DRDY, 5.0);                                 // dans cette ligne je dit à l'ADS tu communique avec le bus Vspi et ton chip select est 5 et le DATA ready est 4 et aussi ta tension d'excitation est de 5 volt.
// ADS1256 ads2(&vspi, CS_PIN 2, DRDY 2, 5.0);                                      // même chose en cas d'un 2 cartes ADS


/****************************************************************_Initialisation_***************************************************************************/ 
void setup() {
    Serial.begin(speed_monitor);
    /************************************************Configuration des borches pour les 2 bus spi***********************************************************/
    vspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI,ADS_1_CS_PIN); 
    hspi.begin(HSPI_SCLK,HSPI_MISO,HSPI_MOSI,SD_PIN);

    // Enregistrement des cartes auprès du Manager _inputReader
    inputReaderManager.addDevice(&ads1);                      // On donne la réference d'adresse du capteur ads1 au Manager
    inputReaderManager.begin();                              //  Le manager initialise l'ADS et crée la tâche FreeRTOS



    //**********************************************************Initialisation de la carte SD***********************************************************************************************/    


}

void loop() {
    /********************/
}