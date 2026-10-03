/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : Main.cpp                        ************************************************************/
/********************************************* Date de dernière modification    : 02/06/2026                      ************************************************************/

#include <Arduino.h>
#include <main.hpp>
#include <BUSspi.hpp>
#include <TaskManager.hpp>
#include <ADS1256.hpp>
#include <WebServer.h>
#include "WiFiManager.hpp"
#include "ButtonManager.hpp"
#include <WiFi.h>
//#include "soc/soc.h"
//#include "soc/rtc_cntl_reg.h"
#include <esp_wifi.h>
#include <SD.h>

#define SAMPLING_PERIOD  10
#define Vexc             5.0

WebServer server(80);

SPIClass vspi(VSPI);
SPIClass hspi(HSPI);

InputReaderManager inputReaderManager(SAMPLING_PERIOD);
InputDevice        ads1(&hspi, ADS_1_CS_PIN, ADS_1_DRDY, Vexc, 1000000, ADS1256_DRATE_500, GAIN_1);
CardSD             myCard(&vspi, SD_PIN, "/Acquisition_Folder/measures.csv");
MemoryManager&     memoryManager = MemoryManager::getInstance();
TaskManager        taskManager(inputReaderManager, memoryManager);
ButtonManager      buttonManager;   //contrôle physique LCD + bouton

SemaphoreHandle_t _sdMutex = NULL;

// Tâche HTTP optimisée pour laisser du temps processeur au service Wi-Fi de l'ESP32
void httpServerTask(void* pvParameters) {
    disableCore0WDT();
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
void setup() {
    //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.begin(115200);
    delay(200);

    _sdMutex = xSemaphoreCreateMutex();

    pinMode(LED_SAFE_EJECT, OUTPUT);
    digitalWrite(LED_SAFE_EJECT, LOW);

    /* Bus SPI SD */
    vspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, SD_PIN);
    if (SD.begin(SD_PIN, vspi)) {
        if (!SD.exists("/Acquisition_Folder")) SD.mkdir("/Acquisition_Folder");
    }

    /* Initialisation du LCD 16x2 */
    WifiManager::initLCD();

    /* LED témoin acquisition (GPIO à assigner — clignote pendant l'enregistrement) */
    WifiManager::initAcqLed();

    /* Contrôle physique — bouton + LCD (démarre après initLCD) */
    buttonManager.begin();

    /* Configuration Wi-Fi */
    WifiManager::setSdMutex(_sdMutex);
    WifiManager::startAP("TEST_TEST", "");
    //esp_wifi_set_ps(WIFI_PS_NONE); // Performance Wi-Fi maximale, pas de mode veille
    WifiManager::setInputReaderManager(&inputReaderManager);
    WifiManager::begin(server);
    server.begin();

    // Serveur HTTP sur le Cœur 0 (partagé avec le Wi-Fi stack)
    xTaskCreatePinnedToCore(httpServerTask, "HTTPTask", 8192, NULL, 1, NULL, 0);

    /* Bus HSPI ADS1256 */
    hspi.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, ADS_1_CS_PIN);
    ads1.begin();

    /* Tâches d'acquisition et d'écriture déplacées sur le Cœur 1 */
    memoryManager.setSdMutex(_sdMutex);
    memoryManager.addDevice(&myCard);
    inputReaderManager.addDevice(&ads1);
    
    inputReaderManager.begin(); // Démarre ReadyTask sur Cœur 1
    memoryManager.begin();      // Démarre SDTask sur Cœur 1
    taskManager.begin();        // Démarre TaskManagerTask sur Cœur 1

    ads1.Set_ADS1256_SPS(ADS1256_DRATE_500);
    //WifiManager::updateSPS(ADS1256_DRATE_100);

    // pour auto_calibration du l'ADS1256
    digitalWrite(CS_PIN, LOW);
    hspi.transfer(ADS1256_IO_DIR_MASK);
    digitalWrite(CS_PIN, HIGH);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(5000));
}