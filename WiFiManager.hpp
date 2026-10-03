#ifndef WIFIMANAGER_HPP
#define WIFIMANAGER_HPP

#include <WebServer.h>
#include <WiFi.h>
#include <SD.h>
#include <LittleFS.h>          // Fichiers web embarqués dans la flash ESP32
#include <freertos/semphr.h>
#include <LiquidCrystal.h>

#define LED_SAFE_EJECT  2
#define LED_ACQ         22   // signaler ou dire que l'enregistrement se fait sur la carte SD
                              // (deplace de 4 vers 22 : reutilisee comme LED "prise de mesure",
                              //  clignote pendant l'ecriture SD, voir initAcqLed() dans WiFiManager.cpp.
                              //  GPIO22 etait le bouton EJECT, desormais deplace sur GPIO13,
                              //  voir BTN_EJECT dans ButtonManager.hpp)

class InputReaderManager;

class WifiManager {
private:
    static float               _currentVoltage;
    static float               _gaugeValues[4];
    static int                 _currentSPS;
    static bool                _isRecordingSD;
    static uint8_t             _sensorMask;
    static bool                _safeEject;
    static InputReaderManager* _inputReaderManager;
    static SemaphoreHandle_t   _sdMutex;
    static uint8_t             _sdCsPin;   // CS de la carte SD (pour ejectReset)
    static TaskHandle_t        _blinkTaskHandle;  // tâche clignotement LED_ACQ

    // LCD 16x2 — instance statique partagée dans toute la classe
    static LiquidCrystal       _lcd;

    static void streamFromSD(WebServer& server,
                              const char* path,
                              const char* contentType,
                              const char* cache = "public, max-age=3600");

    // Sert un fichier depuis la flash interne (LittleFS)
    static void streamFromFlash(WebServer& server,
                                const char* path,
                                const char* contentType,
                                const char* cache = "public, max-age=86400");

    // Mise à jour de l'affichage LCD selon le masque
    static void _updateLCD(uint8_t mask);

public:
    static void startAP(const char* ssid, const char* password);
    static void begin(WebServer& server);
    static void setInputReaderManager(InputReaderManager* mgr);
    static void setSdMutex(SemaphoreHandle_t m);
    static void setSdCsPin(uint8_t cs);     // pin CS de la SD (pour ejectReset)

    // Initialisation du LCD (à appeler dans setup())
    static void initLCD();

    // Initialise la LED témoin acquisition et démarre la tâche de clignotement
    static void initAcqLed();

    static void  updateValue(float val);
    static void  updateGauge(uint8_t index, float val);
    static float getValue();
    static void  updateSPS(int drate);
    static bool  isSafeEject()     { return _safeEject;     }
    static bool  isRecordingSD()   { return _isRecordingSD; }
    static uint8_t getSensorMask() { return _sensorMask;    }

    /* Active / desactive l'ejection securisee depuis le bouton physique     *
     * (GPIO22, voir ButtonManager). Coherent avec les routes HTTP           *
     * /ejectSD et /ejectReset qui ecrivent directement _safeEject.          */
    static void setSafeEject(bool eject) { _safeEject = eject; }

    /* API ButtonManager*/
    /* Écriture directe du masque depuis le bouton physique (thread-safe :   *
     * uint8_t est atomique sur ESP32 Xtensa).                               */
    static void setSensorMask(uint8_t mask) { _sensorMask    = mask & 0x0F; }

    /* Active / désactive l'enregistrement SD depuis le bouton physique.     */
    static void setRecordingSD(bool rec)    { _isRecordingSD = rec; }

    /* Retourne la valeur d'une jauge (index 0-3) pour affichage LCD.        */
    static float getGaugeValue(uint8_t i)   { return (i < 4) ? _gaugeValues[i] : 0.0f; }

    /* Écrit une ligne sur le LCD (row = 0 ou 1, 16 caractères max).         *
     * Utilisé par ButtonManager pour ne pas dépendre de LiquidCrystal.h.   */
    static void lcdPrint(uint8_t row, const char* text);
};

#endif