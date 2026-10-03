/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : WiFiManager.cpp                 ************************************************************/
/********************************************* Date de dernière modification    : 08/06/2026                      ************************************************************/



#include "WiFiManager.hpp"
#include "InputReaderManager.hpp"
#include <LittleFS.h>      

// ── Membres statiques ────────────────────────────────────────────────────────
float               WifiManager::_currentVoltage     = 0;
float               WifiManager::_gaugeValues[4]     = {0, 0, 0, 0};
int                 WifiManager::_currentSPS         = 0;
bool                WifiManager::_isRecordingSD      = false;
bool                WifiManager::_safeEject          = false;
uint8_t             WifiManager::_sensorMask         = 0x0F;
InputReaderManager* WifiManager::_inputReaderManager = nullptr;
SemaphoreHandle_t   WifiManager::_sdMutex            = nullptr;
uint8_t             WifiManager::_sdCsPin             = 255;  // invalide jusqu'à setSdCsPin()
TaskHandle_t        WifiManager::_blinkTaskHandle     = NULL;

/**** LCD 16x2 — RS, E, D4, D5, D6, D7**/
LiquidCrystal WifiManager::_lcd(26, 25, 33, 32, 16, 17);

/******initLCD*****/
void WifiManager::initLCD() {
    _lcd.begin(16, 2);
    _lcd.clear();
    _lcd.setCursor(0, 0);
    _lcd.print("  LIEBHERR DAQ  ");
    _lcd.setCursor(0, 1);
    _lcd.print(" En attente...  ");
}

/*******************_updateLCD()affichage selon le nombre de capteurs actifs***************************/ 
void WifiManager::_updateLCD(uint8_t mask) {
    // Compte les bits actifs dans le masque
    int nb = __builtin_popcount(mask & 0x0F);

    _lcd.clear();
    _lcd.setCursor(0, 0);

    /**Ligne 1 : nombre de capteurs***/
    if (nb == 1) {
        _lcd.print("1 capteur");
    } else {
        _lcd.print(nb);
        _lcd.print(" capteurs");
    }

    /*****Ligne 2 : état****/
    _lcd.setCursor(0, 1);
    _lcd.print("save ON");
}

/***********startAP() ******/
void WifiManager::startAP(const char* ssid, const char* password) {

    // Monte LittleFS (flash interne)
    if (!LittleFS.begin(true)) {   
        Serial.println("[Flash] Erreur montage LittleFS !");
    } else {
        Serial.println("[Flash] LittleFS monté OK");
    }

    WiFi.softAP(ssid, password);
    pinMode(LED_SAFE_EJECT, OUTPUT);
    digitalWrite(LED_SAFE_EJECT, LOW);
}

void WifiManager::setInputReaderManager(InputReaderManager* mgr) { _inputReaderManager = mgr; }
void WifiManager::setSdMutex(SemaphoreHandle_t m)                { _sdMutex = m; }
void WifiManager::setSdCsPin(uint8_t cs)                         { _sdCsPin = cs; }

/*****************************begin()  déclaration de toutes les routes HTTP***/
void WifiManager::begin(WebServer& server) {

    server.on("/setSDConfig", HTTP_POST, [&server]() {
        if (!server.hasArg("mask")) {
            server.send(400, "text/plain", "Missing mask");
            return;
        }

        uint8_t mask   = (uint8_t)(server.arg("mask").toInt() & 0x0F);
        bool    record = true;

        if (server.hasArg("record")) {
            String v = server.arg("record");
            v.toLowerCase();
            record = (v == "true" || v == "1");
        }

        _sensorMask    = mask;
        _isRecordingSD = record;

        /*** Mise à jour du LCD*****/
        if (record && mask != 0) {
            _updateLCD(mask);
        }

        Serial.printf("[WiFi] /setSDConfig → mask=0x%02X record=%d\n", mask, record);
        server.send(200, "text/plain", "OK");
    });

    /******************Arrêt de l'enregistrement****/
    server.on("/stopRecord", HTTP_POST, [&server]() {
        _isRecordingSD = false;

        /*  Affiche l'état d'arrêt sur le LCD*/
        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print("Acquisition");
        _lcd.setCursor(0, 1);
        _lcd.print("arretee");

        Serial.println("[WiFi] Enregistrement SD arrete.");
        server.send(200, "text/plain", "OK");
    });

    /****  Routes gestion SD*************************/ 

    server.on("/ejectSD", HTTP_POST, [&server]() {
        _safeEject     = true;
        _isRecordingSD = false;
        digitalWrite(LED_SAFE_EJECT, HIGH);

        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print("SD ejectee");
        _lcd.setCursor(0, 1);
        _lcd.print("OK retire");

        Serial.println("[WiFi] Safe eject demande.");
        server.send(200, "text/plain", "SAFE");
    });

    /*
     * POST /ejectReset
     * Réactive l'écriture SD après réinsertion de la carte.
     * Réinitialise SD.begin() et éteint la LED.
     */
    server.on("/ejectReset", HTTP_POST, [&server]() {
        _safeEject = false;
        digitalWrite(LED_SAFE_EJECT, LOW);

        /* Réinitialise le bus SD si le CS pin est connu */
        // Note : MemoryManager::beginAll() sera rappelé au prochain cycle
        // car MemoryManager vérifie _safeEject dans run().

        _lcd.clear();
        _lcd.setCursor(0, 0);
        _lcd.print("  LIEBHERR DAQ  ");
        _lcd.setCursor(0, 1);
        _lcd.print(" En attente...  ");

        Serial.println("[WiFi] Eject reset — ecriture SD reactivee.");
        server.send(200, "text/plain", "OK");
    });

    /*
     * GET /ejectStatus
     * Répond "SAFE" ou "WRITING" selon l'état courant.
     */
    server.on("/ejectStatus", HTTP_GET, [&server]() {
        server.send(200, "text/plain", _safeEject ? "SAFE" : "WRITING");
    });

    /*
     * GET /sdState
     * Répond un JSON { "used": <octets>, "total": <octets> }.
     * Prend le mutex SD pour éviter les conflits avec l'écriture.
     */
    server.on("/sdState", HTTP_GET, [&server]() {
        bool gotMutex = false;
        if (_sdMutex) gotMutex = (xSemaphoreTake(_sdMutex, pdMS_TO_TICKS(800)) == pdTRUE);

        uint64_t used  = 0;
        uint64_t total = 0;
        bool     ok    = false;

        if (gotMutex || !_sdMutex) {
            total = SD.totalBytes();
            used  = SD.usedBytes();
            ok    = (total > 0);
            if (gotMutex) xSemaphoreGive(_sdMutex);
        }

        if (!ok) {
            server.send(200, "application/json", "{\"error\":true}");
            return;
        }

        String json = "{\"used\":";
        json += String((uint32_t)(used  / 1024));  
        json += ",\"total\":";
        json += String((uint32_t)(total / 1024));
        json += ",\"unit\":\"Ko\"}";
        server.send(200, "application/json", json);
    });

    /*
     * GET /listFiles
     * Parcourt /Acquisition_Folder et renvoie un tableau JSON
    */
    server.on("/listFiles", HTTP_GET, [&server]() {
        bool gotMutex = false;
        if (_sdMutex) gotMutex = (xSemaphoreTake(_sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE);

        String json = "[";
        bool   first = true;

        File dir = SD.open("/Acquisition_Folder");
        if (dir) {
            File entry = dir.openNextFile();
            while (entry) {
                if (!entry.isDirectory()) {
                    String name = String(entry.name());
                    // entry.name() peut retourner le chemin complet ou juste le nom selon version SDK
                    int lastSlash = name.lastIndexOf('/');
                    if (lastSlash >= 0) name = name.substring(lastSlash + 1);

                    if (!first) json += ",";
                    json += "{\"name\":\"" + name + "\",\"size\":" + String(entry.size()) + "}";
                    first = false;
                }
                entry.close();
                entry = dir.openNextFile();
            }
            dir.close();
        }

        json += "]";
        if (gotMutex) xSemaphoreGive(_sdMutex);
        server.send(200, "application/json", json);
    });

    /*
     * GET /downloadFile?name=<filename>&saveas=<savename>
     * Télécharge un fichier depuis /Acquisition_Folder/<name>.
     * Le header Content-Disposition force le nom de sauvegarde côté navigateur.
     */
    server.on("/downloadFile", HTTP_GET, [&server]() {
        if (!server.hasArg("name")) {
            server.send(400, "text/plain", "Missing name");
            return;
        }

        String name = server.arg("name");
        // Sécurité : interdit les remontées de dossier
        name.replace("..", "");
        name.replace("/",  "");

        String path = "/Acquisition_Folder/" + name;

        bool gotMutex = false;
        if (_sdMutex) gotMutex = (xSemaphoreTake(_sdMutex, pdMS_TO_TICKS(2000)) == pdTRUE);

        if (!SD.exists(path)) {
            if (gotMutex) xSemaphoreGive(_sdMutex);
            server.send(404, "text/plain", "File not found: " + path);
            return;
        }

        File f = SD.open(path, "r");
        if (!f) {
            if (gotMutex) xSemaphoreGive(_sdMutex);
            server.send(500, "text/plain", "Open error");
            return;
        }

        String saveas = server.hasArg("saveas") ? server.arg("saveas") : name;
        server.sendHeader("Content-Disposition",
                          "attachment; filename=\"" + saveas + "\"");
        server.sendHeader("Cache-Control", "no-cache");
        server.streamFile(f, "text/csv");
        f.close();
        if (gotMutex) xSemaphoreGive(_sdMutex);
    });

    /*
     * POST /deleteFile  (form-data: name=<filename>)
     * Supprime /Acquisition_Folder/<name>.
     */
    server.on("/deleteFile", HTTP_POST, [&server]() {
        if (!server.hasArg("name")) {
            server.send(400, "text/plain", "Missing name");
            return;
        }

        String name = server.arg("name");
        name.replace("..", "");
        name.replace("/",  "");

        String path = "/Acquisition_Folder/" + name;

        bool gotMutex = false;
        if (_sdMutex) gotMutex = (xSemaphoreTake(_sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE);

        bool removed = SD.remove(path);
        if (gotMutex) xSemaphoreGive(_sdMutex);

        if (removed) {
            Serial.printf("[WiFi] Fichier supprime : %s\n", path.c_str());
            server.send(200, "text/plain", "OK");
        } else {
            server.send(500, "text/plain", "Delete failed");
        }
    });

    /*****Fichiers statiques (flash interne LittleFS) ***/
  
    server.on("/", HTTP_GET, [&server]() {
        streamFromFlash(server, "/index.html", "text/html");
    });
    server.on("/index.html", HTTP_GET, [&server]() {
        streamFromFlash(server, "/index.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [&server]() {
        streamFromFlash(server, "/style.css", "text/css");
    });
    server.on("/dashboard.js", HTTP_GET, [&server]() {
        streamFromFlash(server, "/dashboard.js", "text/javascript");
    });
    server.on("/uPlot.iife.min.js", HTTP_GET, [&server]() {
        streamFromFlash(server, "/uPlot.iife.min.js", "text/javascript");
    });
    server.on("/uPlot.min.css", HTTP_GET, [&server]() {
        streamFromFlash(server, "/uPlot.min.css", "text/css");
    });

    /***API données temps réel ***/
    server.on("/data", HTTP_GET, [&server]() {
        String json = "{";
        json += "\"sps\":"  + String(_currentSPS)        + ",";
        json += "\"j1\":"   + String(_gaugeValues[0], 6) + ",";
        json += "\"j2\":"   + String(_gaugeValues[1], 6) + ",";
        json += "\"j3\":"   + String(_gaugeValues[2], 6) + ",";
        json += "\"j4\":"   + String(_gaugeValues[3], 6);
        json += "}";
        server.send(200, "application/json", json);
    });
}

/**streamFromSD() — toujours utilisé pour les données d'acquisition**/
void WifiManager::streamFromSD(WebServer& server, const char* path,
                                const char* contentType, const char* /*cache*/) {
    if (_sdMutex && xSemaphoreTake(_sdMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        server.send(503, "text/plain", "SD Busy");
        return;
    }
    if (!SD.exists(path)) {
        if (_sdMutex) xSemaphoreGive(_sdMutex);
        server.send(404, "text/plain", "Not Found");
        return;
    }
    File f = SD.open(path, "r");
    server.streamFile(f, contentType);
    f.close();
    if (_sdMutex) xSemaphoreGive(_sdMutex);
}


void WifiManager::streamFromFlash(WebServer& server, const char* path,
                                   const char* contentType, const char* cache) {
    if (!LittleFS.exists(path)) {
        Serial.printf("[Flash] 404 : %s\n", path);
        server.send(404, "text/plain", String("Not found in flash: ") + path);
        return;
    }
    File f = LittleFS.open(path, "r");
    if (!f) {
        server.send(500, "text/plain", "Flash open error");
        return;
    }
    server.sendHeader("Cache-Control", cache);
    server.streamFile(f, contentType);
    f.close();
}

/**Accesseurs simples **/
void WifiManager::updateValue(float val) {
    _currentVoltage = val;
    _gaugeValues[0] = val;
}
void WifiManager::updateGauge(uint8_t i, float val) {
    if (i < 4) {
        _gaugeValues[i] = val;
        if (i == 0) _currentVoltage = val;
    }
}
float WifiManager::getValue()     { return _currentVoltage; }
void  WifiManager::updateSPS(int) { /* convertisseur DRATE → Hz à compléter */ }

/** initAcqLed() — LED témoin acquisition **/

void WifiManager::--_ojijjjinitAcqLed() {
    if (LED_ACQ == 255) return;   // GPIO non assigné — désactivé
    pinMode(LED_ACQ, OUTPUT);
    digitalWrite(LED_ACQ, LOW);

    xTaskCreatePinnedToCore(
        [](void*) {
            for (;;) {
                if (LED_ACQ == 255) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
                if (_isRecordingSD) {
                    digitalWrite(LED_ACQ, HIGH);
                    vTaskDelay(pdMS_TO_TICKS(250));
                    digitalWrite(LED_ACQ, LOW);
                    vTaskDelay(pdMS_TO_TICKS(250));
                } else {
                    digitalWrite(LED_ACQ, LOW);
                    vTaskDelay(pdMS_TO_TICKS(100));   // polling léger quand inactif
                }
            }
        },
        "AcqBlink",         // nom de la tâche
        1024,               // stack minimal (aucune allocation dynamique)
        nullptr,
        1,                  // priorité basse
        &_blinkTaskHandle,
        1                   // Cœur 1 (acquisition)
    );
}

void WifiManager::lcdPrint(uint8_t row, const char* text) {
    if (row > 1) return;
    _lcd.setCursor(0, row);
    // Écriture + remplissage jusqu'à 16 chars pour effacer les restes
    int written = 0;
    while (text && *text && written < 16) {
        _lcd.write(*text++);
        written++;
    }
    while (written < 16) {
        _lcd.write(' ');
        written++;
    }
}