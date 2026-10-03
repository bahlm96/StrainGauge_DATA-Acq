/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : InputReaderManager.cpp           ************************************************************/
/********************************************* Description                      : Manager des cartes              ***********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 18/06/2026                      ************************************************************/

/* Changelog :
    v0.2.1  --> Ajout Tare / Zérotage
    v0.3.0  --> _bufferReady + isBufferReady() poll
    v0.4.0  --> Sélection capteurs + mode SD + LED
    v0.4.1  --> Correction pics :
                  - Buffer TOUJOURS complet (timestamp + 4 voies)
                  - _recordToSD conditionne uniquement la LED
                  - Masque copie locale atomique une fois par cycle
    v0.4.2  --> Correction pics (finale) :
                  - Serial.printf DÉPLACÉ après endTransaction() (hors SPI)
                  - delayMicroseconds(400) SUPPRIMÉ (redondant avec setChannel)
                  - WifiManager::updateGauge DÉPLACÉ après endTransaction() (hors SPI)
                  - Priorité ReadyTask 2 → 5
                  - _lastValues[4] pour mémoriser les valeurs et les envoyer hors boucle
*/

#include "InputReaderManager.hpp"
#include "InputDevice.hpp"
#include "WiFiManager.hpp"
#include <main.hpp>     // LED_ACQ_PIN


InputReaderManager::InputReaderManager(uint32_t samplingPeriod)
    : _samplingPeriod(samplingPeriod),
      _bufferReady(false),
      _index_buffer(0),
      _tareRequested(false),
      _tareActive(false),
      _activeMask(0x0F),
      _recordToSD(true)
{
    _currentBuffer = _BufferA;
    memset(_offsets,    0, sizeof(_offsets));
    memset(_lastValues, 0, sizeof(_lastValues));
}


void InputReaderManager::begin() {
    if (_isStarted) return;
    pinMode(LED_ACQ_PIN, OUTPUT);
    digitalWrite(LED_ACQ_PIN, LOW);
    /* Priorité 5 — résiste aux interruptions WiFi du Cœur 0 */
    xTaskCreatePinnedToCore(taskWrapper, "ReadyTask", 8192, this, 5, &_taskHandle, 1);
    _isStarted = true;
}


void InputReaderManager::addDevice(InputDevice* device) {
    _devices.push_back(device);
    device->begin();
}

void InputReaderManager::set_sampling_Period(uint32_t value) {
    _samplingPeriod = value;
}

void InputReaderManager::setAllSPS(uint8_t drate) {
    for (InputDevice* device : _devices) {
        device->Set_ADS1256_SPS(drate);
    }
}

void InputReaderManager::requestTare() {
    _tareRequested = true;
}

void InputReaderManager::resetTare() {
    _tareActive    = false;
    _tareRequested = false;
    memset(_offsets, 0, sizeof(_offsets));
}

void InputReaderManager::taskWrapper(void* pvParameters) {
    ((InputReaderManager*)pvParameters)->_run();
}

int InputReaderManager::getActiveSensorCount() const {
    int count = 0;
    for (int i = 0; i < 4; i++) if (_activeMask & (1 << i)) count++;
    return count;
}


/*
  _run() — Boucle principale de lecture (priorité 5, Cœur 1)

  Règles anti-pics :
    1. Serial.printf        → APRÈS endTransaction(), hors SPI
    2. WifiManager::updateGauge → APRÈS endTransaction(), hors SPI
    3. delayMicroseconds(400) → SUPPRIMÉ (setChannel attend déjà DRDY)
    4. readRaw() → timeout 5 ms intégré (voir InputDevice.cpp)
    5. Buffer TOUJOURS complet [timestamp][j1][j2][j3][j4]
    6. Masque lu en copie locale atomique une seule fois par cycle
*/
void InputReaderManager::_run() {

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(_samplingPeriod);

    double raw_mV      = 0.0;
    double value_in_mV = 0.0;
    double timestamp   = 0.0;

    while (true) {

        /* ── Copie locale atomique — une seule lecture par cycle ─────────── */
        uint8_t localMask  = WifiManager::getSensorMask();
        bool    localRecSD = WifiManager::isRecordingSD();
        _activeMask = localMask;
        _recordToSD = localRecSD;

        /* ── LED — hors boucle SPI ───────────────────────────────────────── */
        if (localRecSD) {
            if (++_ledToggleCount >= 50) {
                _ledToggleCount = 0;
                _ledState = !_ledState;
                digitalWrite(LED_ACQ_PIN, _ledState ? HIGH : LOW);
            }
        } else {
            _ledState = false; _ledToggleCount = 0;
            digitalWrite(LED_ACQ_PIN, LOW);
        }

        /* ── Timestamp — TOUJOURS écrit ──────────────────────────────────── */
        _currentBuffer[_index_buffer] = timestamp;
        timestamp += _samplingPeriod;
        _index_buffer++;
        if (_index_buffer >= BUFFER_SIZE) {
            _switchBuffer(); _bufferReady = true; _index_buffer = 0;
        }

        bool doTareCapture = _tareRequested;

        /* ══════════════════════════════════════════════════════════════════
           BOUCLE SPI — rien d'autre que du SPI ici
           Pas de Serial, pas de WiFi, pas de delay inutile
           ══════════════════════════════════════════════════════════════════ */
        for (InputDevice* device : _devices) {
            device->beginTransaction();

            for (int i = 0, idcapteur = 1; i <= 6; i += 2, idcapteur++) {

                /* 1. Changement canal — setChannel() attend déjà DRDY */
                device->setChannel(i, i + 1);
                /* ← PAS de delayMicroseconds(400) ici */

                /* 2. Lecture avec timeout (voir InputDevice.cpp) */
                raw_mV = device->readRaw() * device->getQuantum();

                /* 3. Tare */
                if (doTareCapture) _offsets[idcapteur - 1] = raw_mV;

                /* 4. Calcul valeur finale */
                if (doTareCapture || _tareActive)
                    value_in_mV = raw_mV - _offsets[idcapteur - 1];
                else
                    value_in_mV = raw_mV;

                /* 5. Mémorisation pour envoi APRÈS la transaction */
                _lastValues[idcapteur - 1] = value_in_mV;

                /* 6. Stockage buffer — TOUJOURS */
                _currentBuffer[_index_buffer] = value_in_mV;
                _index_buffer++;
                if (_index_buffer >= BUFFER_SIZE) {
                    _switchBuffer(); _bufferReady = true; _index_buffer = 0;
                }
            }

            device->endTransaction();
        }
        /* ══════════════════════════════════════════════════════════════════
           FIN BOUCLE SPI
           ══════════════════════════════════════════════════════════════════ */

        /* ── Teleplot + WiFi — UNIQUEMENT ici, après endTransaction() ────── */
        for (int i = 0; i < 4; i++) {
            Serial.printf(">Jauge %d:%.6f\n", i + 1, _lastValues[i]);
            if (localMask & (1 << i)) {
                WifiManager::updateGauge(i, (float)_lastValues[i]);
            }
        }

        /* ── Confirmation tare ───────────────────────────────────────────── */
        if (doTareCapture) {
            _tareActive    = true;
            _tareRequested = false;
            Serial.printf("[TARE] J1:%.6f  J2:%.6f  J3:%.6f  J4:%.6f\n",
                          _offsets[0], _offsets[1], _offsets[2], _offsets[3]);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


void InputReaderManager::_switchBuffer() {
    if (_currentBuffer == _BufferA) {
        _fullBufferPtr = _BufferA; _currentBuffer = _BufferB;
    } else {
        _fullBufferPtr = _BufferB; _currentBuffer = _BufferA;
    }
}

bool InputReaderManager::isBufferReady() {
    if (_bufferReady) { _bufferReady = false; return true; }
    return false;
}

double* InputReaderManager::getBufferReady() { return _fullBufferPtr; }
int     InputReaderManager::getBufferSize()  { return BUFFER_SIZE;    }