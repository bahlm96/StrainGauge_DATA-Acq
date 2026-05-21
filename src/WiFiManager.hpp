#ifndef WIFIMANAGER_HPP
#define WIFIMANAGER_HPP

#include <WebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

#define LED_SAFE_EJECT 26
  


class InputReaderManager;


class WifiManager {
private:
    static float              _currentVoltage;
    static int                _currentSPS;
    static bool               _isRecordingSD;
    static bool               _safeEject;

    /* Pointeur vers le gestionnaire de lecture, injecté depuis main.cpp
       via setInputReaderManager() avant d'appeler begin().              */
    static InputReaderManager* _inputReaderManager;

public:
    /* ── Init ─────────────────────────────────────────────────────────── */
    static void startAP(const char* ssid, const char* password);
    static void begin(WebServer& server);

    /* ── Injection de dépendance ──────────────────────────────────────── */
    /* Doit être appelé dans main.cpp AVANT WifiManager::begin(server).   */
    static void setInputReaderManager(InputReaderManager* mgr);

    /* ── Mises à jour des valeurs partagées ──────────────────────────── */
    static void  updateValue(float val);
    static float getValue();
    static void  updateSPS(int drate);

    /* ── Flags d'état ────────────────────────────────────────────────── */
    static bool isSafeEject() { return _safeEject; }
};

#endif
