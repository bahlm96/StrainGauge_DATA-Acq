/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : InputReaderManager.cpp           ************************************************************/
/********************************************* Description                      : Manager des cartes              ***********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 05/06/2026                      ************************************************************/

/* Changelog :
    v0.2.1  --> Ajout du système de Tare / Zérotage :
                  - requestTare() : capture les offsets bruts à la prochaine itération de _run()
                  - resetTare()   : remet les offsets à 0 et désactive la soustraction
                  - Dans _run()   : si tare demandée → stocke raw_mV comme offset puis soustrait (résultat = 0 au premier cycle)
                                    si tare active    → soustrait _offsets[voie] de chaque lecture
    v0.3.0  --> Découplage SD / WiFi :
                  - Suppression du flag _bufferReady et de isBufferReady() (poll 50 ms dans TaskManager)
                  - _switchBuffer() appelle xTaskNotifyGive(_storageTaskHandle) directement
                    → TaskManager se réveille en < 1 ms au lieu de 0..50 ms
                  - TaskManager tourne en ulTaskNotifyTake(pdTRUE, portMAX_DELAY) : zéro CPU quand rien à écrire
    v0.4.0  --> Sélection capteurs + mode enregistrement SD :
                  - _run() consulte WifiManager::getSensorMask() et WifiManager::isRecordingSD()
                  - updateGauge() : uniquement pour les capteurs dont le bit est actif dans le masque
                  - Stockage buffer SD : uniquement si mode SD actif (both | sd)
                  - Structure buffer dynamique : [timestamp] + N valeurs selon les bits actifs du masque
                    → getActiveSensorCount() retourne le nombre de capteurs actifs
                    → getActiveMask() retourne le masque courant (pour le TaskManager)
                  - Le TaskManager appelle getActiveMask() pour construire l'en-tête CSV correct
*/

#include "InputReaderManager.hpp"
#include "InputDevice.hpp"
#include "WiFiManager.hpp"
#include <main.hpp>     // LED_ACQ_PIN


/* Constructeur initialise le double buffer, les flags de synchronisation et le tableau d'offsets à zéro */
InputReaderManager::InputReaderManager(uint32_t samplingPeriod)
    : _samplingPeriod(samplingPeriod),
      _index_buffer(0),
      _tareRequested(false),
      _tareActive(false),
      _activeMask(0x0F),        // tous les capteurs actifs par défaut
      _recordToSD(true)
{
    _currentBuffer = _BufferA;
    memset(_offsets, 0, sizeof(_offsets));
}


/* begin() — Lance la tâche FreeRTOS de lecture (une seule fois) */
void InputReaderManager::begin() {
    if (_isStarted) return;
    pinMode(LED_ACQ_PIN, OUTPUT);
    digitalWrite(LED_ACQ_PIN, LOW);
    xTaskCreatePinnedToCore(taskWrapper, "ReadyTask", 8192, this, 2, &_taskHandle, 1);
    _isStarted = true;
}


/* addDevice() — Enregistre un ADS1256 et l'initialise immédiatement */
void InputReaderManager::addDevice(InputDevice* device) {
    _devices.push_back(device);
    device->begin();
}


/* set_sampling_Period() — Modifie dynamiquement la période FreeRTOS (en ms) */
void InputReaderManager::set_sampling_Period(uint32_t value) {
    _samplingPeriod = value;
}

/* setAllSPS() — Applique un code registre DRATE à tous les ADS1256 enregistrés */
void InputReaderManager::setAllSPS(uint8_t drate) {
    for (InputDevice* device : _devices) {
        device->Set_ADS1256_SPS(drate);
    }
}


/* Tare : requestTare() */
void InputReaderManager::requestTare() {
    _tareRequested = true;
}


/* Tare : resetTare() */
void InputReaderManager::resetTare() {
    _tareActive    = false;
    _tareRequested = false;
    memset(_offsets, 0, sizeof(_offsets));
}


/* taskWrapper() — Adaptateur C pour FreeRTOS */
void InputReaderManager::taskWrapper(void* pvParameters) {
    InputReaderManager *self = (InputReaderManager*) pvParameters;
    self->_run();
}


/* getActiveSensorCount() — Nombre de bits à 1 dans le masque actif */
int InputReaderManager::getActiveSensorCount() const {
    int count = 0;
    for (int i = 0; i < 4; i++) {
        if (_activeMask & (1 << i)) count++;
    }
    return count;
}


/*
  _run() — Boucle principale de lecture (tâche FreeRTOS, priorité 2)

  Structure du buffer par cycle :
      [timestamp] [val_capteurX] [val_capteurY] ...
      Seuls les capteurs dont le bit est actif dans _activeMask sont inclus.
      Ex : masque 0b0101 (J1 + J3) → [timestamp] [j1] [j3]   (3 doubles par cycle)

  Le masque et le mode SD sont lus depuis WifiManager à chaque cycle
  pour prendre en compte les changements depuis l'IHM sans redémarrage.

  Logique de tare : inchangée (opère sur les 4 voies indépendamment du masque).
*/
void InputReaderManager::_run() {

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(_samplingPeriod);

    double raw_mV      = 0.0;
    double value_in_mV = 0.0;
    double timestamp   = 0.0;

    while (true) {

        /* Lecture du masque et du mode SD depuis WifiManager (mis à jour par l'IHM) */
        _activeMask  = WifiManager::getSensorMask();
        _recordToSD  = WifiManager::isRecordingSD();

        /* ── LED acquisition — clignote à ~2 Hz quand enregistrement actif ──── */
        /* _ledToggleCount s'incrémente à chaque cycle (10 ms).                  *
         * 50 cycles × 10 ms = 500 ms → bascule toutes les 500 ms → 1 Hz visuel */
        if (_recordToSD) {
            _ledToggleCount++;
            if (_ledToggleCount >= 50) {
                _ledToggleCount = 0;
                _ledState = !_ledState;
                digitalWrite(LED_ACQ_PIN, _ledState ? HIGH : LOW);
            }
        } else {
            /* Acquisition arrêtée → LED éteinte */
            _ledState       = false;
            _ledToggleCount = 0;
            digitalWrite(LED_ACQ_PIN, LOW);
        }

        /* ── Timestamp ──────────────────────────────────────────────────────── */
        /* Le timestamp est toujours écrit en premier slot du cycle,
           uniquement si l'enregistrement SD est actif.                         */
        if (_recordToSD) {
            _currentBuffer[_index_buffer] = timestamp;
            _index_buffer++;
            if (_index_buffer >= BUFFER_SIZE) {
                _switchBuffer();
                _index_buffer = 0;
            }
        }
        timestamp += _samplingPeriod;

        bool doTareCapture = _tareRequested;

        /* ── Lecture de toutes les voies ────────────────────────────────────── */
        for (InputDevice* device : _devices) {

            device->beginTransaction();

            for (int i = 0, idcapteur = 1; i <= 6; i += 2, idcapteur++) {

                bool capteurActif = (_activeMask & (1 << (idcapteur - 1))) != 0;

                /* 1. Lecture brute — toujours effectuée (séquence SPI obligatoire sur ADS1256) */
                device->setChannel(i, i + 1);
                delayMicroseconds(400);
                raw_mV = device->readRaw() * device->getQuantum();

                /* 2. Capture de l'offset si tare demandé */
                if (doTareCapture) {
                    _offsets[idcapteur - 1] = raw_mV;
                }

                /* 3. Soustraction de l'offset */
                if (doTareCapture || _tareActive) {
                    value_in_mV = raw_mV - _offsets[idcapteur - 1];
                } else {
                    value_in_mV = raw_mV;
                }
                //Serial.printf(">Jauge %d : %.6f\n", idcapteur, value_in_mV);
                /* 4. Mise à jour interface Web — uniquement si capteur actif */
                if (capteurActif) {
                    WifiManager::updateGauge(idcapteur - 1, (float)value_in_mV);
                }

                /* 5. Stockage SD — uniquement si capteur actif ET mode SD activé */
                if (capteurActif && _recordToSD) {
                    _currentBuffer[_index_buffer] = value_in_mV;
                    _index_buffer++;
                    if (_index_buffer >= BUFFER_SIZE) {
                        _switchBuffer();
                        _index_buffer = 0;
                    }
                }
            }

            device->endTransaction();
        }

        /* Confirmation du tare après lecture complète des 4 voies */
        if (doTareCapture) {
            _tareActive    = true;
            _tareRequested = false;
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


/* _switchBuffer() — Bascule entre BufferA et BufferB et réveille TaskManager */
void InputReaderManager::_switchBuffer() {
    if (_currentBuffer == _BufferA) {
        _fullBufferPtr = _BufferA;
        _currentBuffer = _BufferB;
    } else {
        _fullBufferPtr = _BufferB;
        _currentBuffer = _BufferA;
    }

    if (_storageTaskHandle != NULL) {
        xTaskNotifyGive(_storageTaskHandle);
    }
}


double* InputReaderManager::getBufferReady()      { return _fullBufferPtr; }
int     InputReaderManager::getBufferSize()        { return BUFFER_SIZE;    }
uint8_t InputReaderManager::getActiveMask()  const { return _activeMask;    }