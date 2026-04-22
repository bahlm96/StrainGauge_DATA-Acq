#include "WiFiManager.hpp"
#include "InputReaderManager.hpp"   // définition complète nécessaire pour appeler requestTare() / resetTare()
#include <SD.h>

/* ── Initialisation des variables statiques ──────────────────────────────── */
float              WifiManager::_currentVoltage      = 0;
int                WifiManager::_currentSPS          = 0;
bool               WifiManager::_isRecordingSD       = true;
bool               WifiManager::_safeEject           = false;
InputReaderManager* WifiManager::_inputReaderManager = nullptr;


/* ── setInputReaderManager() ─────────────────────────────────────────────── */
void WifiManager::setInputReaderManager(InputReaderManager* mgr) {
    _inputReaderManager = mgr;
}


/* ── startAP() ───────────────────────────────────────────────────────────── */
void WifiManager::startAP(const char* ssid, const char* password) {
    WiFi.softAP(ssid, password);
    Serial.print("IP AP: ");
    Serial.println(WiFi.softAPIP());
}


/* ── Mises à jour partagées ─────────────────────────────────────────────── */
void  WifiManager::updateValue(float val) { _currentVoltage = val; }
float WifiManager::getValue()             { return _currentVoltage; }


/* ── begin() — Enregistrement de toutes les routes HTTP ─────────────────── */
void WifiManager::begin(WebServer& server) {

    /* ── Fichiers statiques LittleFS ────────────────────────────────────── */

    /* index.html */
    server.on("/", HTTP_GET, [&server]() {
        if (LittleFS.exists("/index.html")) {
            File f = LittleFS.open("/index.html", "r");
            server.streamFile(f, "text/html");
            f.close();
        } else {
            server.send(200, "text/plain", "Serveur OK — index.html absent.");
        }
    });

    /* style.css — envoyé avec le bon Content-Type et un cache de 10 min */
    server.on("/style.css", HTTP_GET, [&server]() {
        if (LittleFS.exists("/style.css")) {
            server.sendHeader("Cache-Control", "public, max-age=600");
            File f = LittleFS.open("/style.css", "r");
            server.streamFile(f, "text/css");
            f.close();
        } else {
            server.send(404, "text/plain", "style.css introuvable.");
        }
    });

    /* dashboard.js — envoyé avec le bon Content-Type et un cache de 10 min */
    server.on("/dashboard.js", HTTP_GET, [&server]() {
        if (LittleFS.exists("/dashboard.js")) {
            server.sendHeader("Cache-Control", "public, max-age=600");
            File f = LittleFS.open("/dashboard.js", "r");
            server.streamFile(f, "application/javascript");
            f.close();
        } else {
            server.send(404, "text/plain", "dashboard.js introuvable.");
        }
    });


    /* ── Lecture des valeurs courantes ──────────────────────────────────── */
    server.on("/getSPS", HTTP_GET, [&server]() {
        server.send(200, "text/plain", String(_currentSPS));
    });

    server.on("/getVoltage", HTTP_GET, [&server]() {
        server.send(200, "text/plain", String(_currentVoltage, 6));
    });


    /* ── Toggle enregistrement SD ───────────────────────────────────────── */
    server.on("/toggleSD", HTTP_POST, [&server]() {
        _isRecordingSD = !_isRecordingSD;
        server.send(200, "text/plain", _isRecordingSD ? "RECORDING" : "STOPPED");
    });


    /* ────────────────────────────────────────────────────────────────────────
       TARE — POST /tare
       Déclenche la capture des offsets sur les 4 voies au prochain cycle
       de lecture FreeRTOS (via le flag volatile _tareRequested).
       Réponse JSON : {"status":"ok","message":"Tare en cours..."}
    ──────────────────────────────────────────────────────────────────────── */
    server.on("/tare", HTTP_POST, [&server]() {
        if (_inputReaderManager == nullptr) {
            server.send(503, "application/json",
                        "{\"status\":\"error\",\"message\":\"InputReaderManager non connecté.\"}");
            return;
        }
        _inputReaderManager->requestTare();
        server.send(200, "application/json",
                    "{\"status\":\"ok\",\"message\":\"Tare en cours — offsets capturés au prochain cycle.\"}");
    });


    /* ────────────────────────────────────────────────────────────────────────
       RESET TARE — POST /resetTare
       Remet tous les offsets à zéro et désactive la soustraction.
       Réponse JSON : {"status":"ok","message":"Tare réinitialisé."}
    ──────────────────────────────────────────────────────────────────────── */
    server.on("/resetTare", HTTP_POST, [&server]() {
        if (_inputReaderManager == nullptr) {
            server.send(503, "application/json",
                        "{\"status\":\"error\",\"message\":\"InputReaderManager non connecté.\"}");
            return;
        }
        _inputReaderManager->resetTare();
        server.send(200, "application/json",
                    "{\"status\":\"ok\",\"message\":\"Tare réinitialisé — soustraction désactivée.\"}");
    });


    /* ────────────────────────────────────────────────────────────────────────
       STATUT TARE — GET /tareStatus
       Retourne l'état du tare et les 4 offsets capturés.
       Réponse JSON :
         {
           "active": true,
           "offsets": [0.001234, -0.000021, 0.000876, 0.000012]
         }
    ──────────────────────────────────────────────────────────────────────── */
    server.on("/tareStatus", HTTP_GET, [&server]() {
        if (_inputReaderManager == nullptr) {
            server.send(503, "application/json",
                        "{\"status\":\"error\",\"message\":\"InputReaderManager non connecté.\"}");
            return;
        }

        const double* off    = _inputReaderManager->getOffsets();
        bool          active = _inputReaderManager->isTareActive();

        /* Construction manuelle du JSON (pas de bibliothèque ArduinoJson requise) */
        String json = "{";
        json += "\"active\":" + String(active ? "true" : "false") + ",";
        json += "\"offsets\":[";
        for (int i = 0; i < NB_CHANNELS; i++) {
            json += String(off[i], 6);
            if (i < NB_CHANNELS - 1) json += ",";
        }
        json += "]}";

        server.sendHeader("Cache-Control", "no-cache");
        server.send(200, "application/json", json);
    });


    /* ── Éjection sécurisée SD ──────────────────────────────────────────── */
    server.on("/ejectSD", HTTP_POST, [&server]() {
        _safeEject = !_safeEject;
        digitalWrite(LED_SAFE_EJECT, _safeEject ? HIGH : LOW);
        Serial.printf("[SD] Éjection : %s\n", _safeEject ? "ON" : "OFF");
        server.send(200, "text/plain", _safeEject ? "SAFE" : "WRITING");
    });

    server.on("/ejectStatus", HTTP_GET, [&server]() {
        server.send(200, "text/plain", _safeEject ? "SAFE" : "WRITING");
    });


    /* ── Liste des fichiers CSV sur la SD ───────────────────────────────── */
    server.on("/listFiles", HTTP_GET, [&server]() {
        String json = "[";
        File root = SD.open("/");
        if (root) {
            bool first = true;
            File entry = root.openNextFile();
            while (entry) {
                String fname = String(entry.name());
                if (!entry.isDirectory() &&
                    (fname.endsWith(".csv") || fname.endsWith(".CSV"))) {
                    if (!first) json += ",";
                    json += "{\"name\":\"" + fname + "\","
                            "\"size\":"   + String(entry.size()) + "}";
                    first = false;
                }
                entry.close();
                entry = root.openNextFile();
            }
            root.close();
        }
        json += "]";
        server.sendHeader("Cache-Control", "no-cache");
        server.send(200, "application/json", json);
    });


    /* ── Téléchargement d'un fichier CSV ────────────────────────────────── */
    server.on("/downloadFile", HTTP_GET, [&server]() {
        if (!server.hasArg("name")) {
            server.send(400, "text/plain", "Paramètre 'name' manquant.");
            return;
        }
        String fname = "/" + server.arg("name");
        if (!SD.exists(fname)) {
            server.send(404, "text/plain", "Fichier introuvable.");
            return;
        }
        File f = SD.open(fname, FILE_READ);
        if (!f) { server.send(500, "text/plain", "Erreur ouverture."); return; }
        server.sendHeader("Content-Disposition",
                          "attachment; filename=\"" + server.arg("name") + "\"");
        server.sendHeader("Cache-Control", "no-cache");
        server.streamFile(f, "text/csv");
        f.close();
        Serial.printf("[SD] Download: %s\n", fname.c_str());
    });


    /* ── Suppression d'un fichier CSV ───────────────────────────────────── */
    server.on("/deleteFile", HTTP_POST, [&server]() {
        if (!server.hasArg("name")) {
            server.send(400, "text/plain", "Paramètre 'name' manquant.");
            return;
        }
        if (_safeEject) {
            server.send(403, "text/plain", "EJECT_ACTIVE");
            return;
        }
        String fname = "/" + server.arg("name");
        if (!SD.exists(fname)) { server.send(404, "text/plain", "Introuvable."); return; }
        if (SD.remove(fname)) {
            Serial.printf("[SD] Supprimé: %s\n", fname.c_str());
            server.send(200, "text/plain", "OK");
        } else {
            server.send(500, "text/plain", "Échec suppression.");
        }
    });
}


/* ── updateSPS() — Conversion code registre DRATE → SPS lisible ─────────── */
void WifiManager::updateSPS(int drate) {
    switch (drate) {
        case 0xF0: _currentSPS = 30000; break;
        case 0xE0: _currentSPS = 15000; break;
        case 0xD0: _currentSPS = 7500;  break;
        case 0xC0: _currentSPS = 3750;  break;
        case 0xB0: _currentSPS = 2000;  break;
        case 0xA1: _currentSPS = 1000;  break;
        case 0x92: _currentSPS = 500;   break;
        case 0x82: _currentSPS = 100;   break;
        case 0x72: _currentSPS = 60;    break;
        case 0x63: _currentSPS = 50;    break;
        case 0x53: _currentSPS = 30;    break;
        case 0x43: _currentSPS = 25;    break;
        case 0x33: _currentSPS = 10;    break;
        case 0x23: _currentSPS = 5;     break;
        case 0x13: _currentSPS = 2;     break;
        default:   _currentSPS = 0;     break;
    }
}
