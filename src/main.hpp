/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : Main.hpp                        ******************************************************/
/********************************************* Description                      : Connexion bus spi               ***********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      ************************************************************/



/****************************************************************************************************************************************************************************/

#ifndef MAIN_HPP
#define MAIN_HPP
#include <SPI.h>
#include <BUSspi.hpp>
#include <MemoryManager.hpp>
#include <InputReaderManager.hpp>

/************************** BUS VSPI ADS1256 **********************************************/
#define HSPI_SCLK 14                   // Broche Horloge 
#define HSPI_MISO 12                   // Broche pour communiquer de l'ads vers esp32
#define HSPI_MOSI 13                   // l'esp32 communique vers l'ADS
#define SPI_SPEED 1000000              // vitesse de communication du bus spi par défault

#define ADS_1_DRDY   4                 // Broche DATA Ready
#define ADS_1_CS_PIN 27                 // Borche chip select de l'ads 1256

/************************** Bouton physique (ButtonManager) ******************/

#define BTN_PIN      15

/************************** LED Acquisition (clignote pendant la mesure) *****/

#define LED_ACQ_PIN  22

//extern SPIClass vspi;                 // éviter de crée un bus spi plusieurs fois de suite, externe pour dire que le bus est déja crée dans un autre fichier ailleur

/************************** BUS HSPI Carte  **********************************************/
#define VSPI_SCLK 18                   // Broche Horloge
#define VSPI_MISO 19                   // Broche pour communiquer de Carte SD vers esp32
#define VSPI_MOSI 23                   // l'esp32 communique vers la carte SD
#define SD_PIN    5                   // Chip select de la carte sd
//extern SPIClass hspi;

#endif