/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : Main.cpp                        ************************************************************/
/********************************************* Description                      : Creation d'objets et initialisation***********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
<<<<<<< HEAD
/********************************************* Date de dernière modification    : 21/05/2026                      ************************************************************/
=======
/********************************************* Date de dernière modification    : 21/04/2026                      ************************************************************/
>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e

/* Changelog :
    v0.2.0  --> Ajout de WifiManager::setInputReaderManager() pour brancher les routes /tare, /resetTare, /tareStatus
*/

#include <Arduino.h>
#include <main.hpp>
#include <BUSspi.hpp>
#include <TaskManager.hpp>
#include <ADS1256.hpp>
#include <LittleFS.h>
#include <WebServer.h>
#include "WiFiManager.hpp"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
<<<<<<< HEAD
#include <esp_wifi.h>
=======
>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e


#define SAMPLING_PERIOD  10          // Période de lecture FreeRTOS (ms)
#define SPEED_MONITOR    115200
#define Vexc             5.0

const char* filename = "/cardSD.csv";
<<<<<<< HEAD
#define BUTTON_PROTECTOR 2
WebServer server(80);

=======
#define BUTTON_PROTECTOR 34

WebServer server(80);

>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e
/* ── Bus SPI ────────────────────────────────────────────────────────────── */
SPIClass vspi(VSPI);
SPIClass hspi(HSPI);

/* ── Objets métier ──────────────────────────────────────────────────────── */
InputReaderManager inputReaderManager(SAMPLING_PERIOD);
<<<<<<< HEAD
InputDevice        ads1(&hspi, ADS_1_CS_PIN, ADS_1_DRDY, Vexc, 1000000, ADS1256_DRATE_1000, GAIN_1);
=======
InputDevice        ads1(&hspi, ADS_1_CS_PIN, ADS_1_DRDY, Vexc, 1000000, ADS1256_DRATE_500, GAIN_1);
>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e
CardSD             myCard(&vspi, SD_PIN, "/measures.csv");
MemoryManager&     memoryManager = MemoryManager::getInstance();
TaskManager        taskManager(inputReaderManager, memoryManager);


void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
<<<<<<< HEAD
    
=======
>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e
    Serial.begin(115200);

    pinMode(LED_SAFE_EJECT, OUTPUT);
    digitalWrite(LED_SAFE_EJECT, LOW);

    /* ── LittleFS + WiFi ─────────────────────────────────────────────── */
    if (!LittleFS.begin(true)) { Serial.println("FS Error"); }

    WifiManager::startAP("TEST_TEST", "");
<<<<<<< HEAD
    esp_wifi_set_ps(WIFI_PS_NONE);                              // pour que le wifi ne se met pas en mode économie d'énergie ( s'eteindre à chaque fois)
    WifiManager::setInputReaderManager(&inputReaderManager);
=======

    /* ── Injection de dépendance : doit être fait AVANT begin() ────────
       Permet aux routes /tare, /resetTare et /tareStatus d'accéder
       directement à inputReaderManager sans variable globale externe.  */
    WifiManager::setInputReaderManager(&inputReaderManager);

>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e
    WifiManager::begin(server);
    server.begin();

    /* ── Bus SPI ─────────────────────────────────────────────────────── */
    vspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, SD_PIN);
    hspi.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, ADS_1_CS_PIN);
    ads1.begin();

    /* ── Démarrage des tâches FreeRTOS ───────────────────────────────── */
    memoryManager.addDevice(&myCard);
    inputReaderManager.addDevice(&ads1);
    inputReaderManager.begin();
    memoryManager.begin();
    taskManager.begin();

    /* ── Configuration ADS1256 ───────────────────────────────────────── */
<<<<<<< HEAD
    ads1.Set_ADS1256_SPS(ADS1256_DRATE_1000);
    WifiManager::updateSPS(ADS1256_DRATE_1000);
=======
    ads1.Set_ADS1256_SPS(ADS1256_DRATE_30000);
    WifiManager::updateSPS(ADS1256_DRATE_30000);
>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e

    /* ── Auto-calibration ────────────────────────────────────────────── */
    digitalWrite(CS_PIN, LOW);
    hspi.transfer(0xF0);    // Commande SELFCAL
    digitalWrite(CS_PIN, HIGH);
}


void loop() {
<<<<<<< HEAD
   server.handleClient();
=======
    server.handleClient();
>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e
    vTaskDelay(pdMS_TO_TICKS(2));
}
