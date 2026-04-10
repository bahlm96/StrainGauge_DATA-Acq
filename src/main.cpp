/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : Main.cpp                        ************************************************************/
/********************************************* Description                      : Creation d'objets et initialisation***********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      ************************************************************/

/*********déclarer les objets, de configurer les bus SPI et de mettre en relation les capteurs avec le gestionnaire de tâches qu'est InputReaderManager*********/

#include <Arduino.h>
#include <main.hpp>
#include <TaskManager.hpp>
#include <ADS1256.hpp>
#define SAMPLING_PERIOD 100
#define SPEED_MONITOR 115200                                        // le port série pour le débug
#define Vexc 5.0        


const char *filename = "/cardSD.csv";                              // créee un fichier en format (.csv)
  
#define BUTTON_PROTECTOR 34                                        // Button poussoir


/*Initialisation des deux BUS SPI de l'ESP32*/
SPIClass vspi(VSPI);
SPIClass hspi(HSPI);


InputReaderManager inputReaderManager(SAMPLING_PERIOD);                            
InputDevice ads1(&vspi, ADS_1_CS_PIN, ADS_1_DRDY, Vexc, 100000, ADS1256_DRATE_500, GAIN_1);                  // dans cette ligne je dit à l'ADS tu communique avec le bus Vspi et ton chip select est 5 et le DATA ready est 4 et aussi ta tension d'excitation est de 5 volt.
CardSD myCard(&hspi, SD_PIN, "/measures.csv");                                                              // creation de l'objet mycard
MemoryManager& memoryManager= MemoryManager::getInstance();
TaskManager taskManager(inputReaderManager, memoryManager);

/*void HandlSDButton(){
    MemoryManager* moi = MemoryManager::getInstance();
    if(!moi->isProtected()){
        moi->enableSafeEject();
        Serial.println("My SD is SAFE");
    } else {

        moi->resetSafeEject();
        Serial.println("SD is on writting mode ");
    }
}*/

void setup() {
    Serial.begin(SPEED_MONITOR);
    /*pinMode(BUTTON_PROTECTOR, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleSDButton, FALLING);*/
    /************************************************Configuration des borches pour les 2 bus spi***********************************************************/
    vspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI,ADS_1_CS_PIN); 
    hspi.begin(HSPI_SCLK,HSPI_MISO,HSPI_MOSI,SD_PIN);
    
    inputReaderManager.addDevice(&ads1);                                           // Enregistrement des cartes auprès du Manager _inputReader
    //ads1.Set_ADS1256_SPS(ADS1256_DRATE_500);
   
    
    if (myCard.begin()) {
        memoryManager.addDevice(&myCard);
        Serial.println("SD OK");
    }
    
    
    inputReaderManager.begin();
    memoryManager.begin();    
    taskManager.begin();
   //Serial.println("Tâches lancées, attente du remplissage du buffer");

}

void loop() {


}