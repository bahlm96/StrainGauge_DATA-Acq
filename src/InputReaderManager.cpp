/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : InputReaderManager.cpp           ************************************************************/
/********************************************* Description                      : Manager des cartes              ***********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 21/04/2026                      ************************************************************/

/* Changelog :
    v0.2.0  --> Ajout du système de Tare / Zérotage :
                  - requestTare() : capture les offsets bruts à la prochaine itération de _run()
                  - resetTare()   : remet les offsets à 0 et désactive la soustraction
                  - Dans _run()   : si tare demandée → stocke raw_mV comme offset puis soustrait (résultat = 0 au premier cycle)
                                    si tare active    → soustrait _offsets[voie] de chaque lecture
*/

#include "InputReaderManager.hpp"
#include "InputDevice.hpp"
#include "WiFiManager.hpp"


/*------------------------------------------------------------------------------------------------------------------
  Constructeur
  Initialise le double buffer, les flags de synchronisation et le tableau d'offsets à zéro.
------------------------------------------------------------------------------------------------------------------*/
InputReaderManager::InputReaderManager(uint32_t samplingPeriod)
    : _samplingPeriod(samplingPeriod),
      _bufferReady(false),
      _index_buffer(0),
      _tareRequested(false),
      _tareActive(false)
{
    _currentBuffer = _BufferA;
    memset(_offsets, 0, sizeof(_offsets));      // Initialise tous les offsets à 0.0
}


/*------------------------------------------------------------------------------------------------------------------
  begin() — Lance la tâche FreeRTOS de lecture (une seule fois)
------------------------------------------------------------------------------------------------------------------*/
void InputReaderManager::begin() {
    if (_isStarted) return;
    xTaskCreatePinnedToCore(taskWrapper, "ReadyTask", 8192, this, 2, &_taskHandle, 1);
    _isStarted = true;
}


/*------------------------------------------------------------------------------------------------------------------
  addDevice() — Enregistre un ADS1256 et l'initialise immédiatement
------------------------------------------------------------------------------------------------------------------*/
void InputReaderManager::addDevice(InputDevice* device) {
    _devices.push_back(device);
    device->begin();
}


/*------------------------------------------------------------------------------------------------------------------
  set_sampling_Period() — Modifie dynamiquement la période FreeRTOS (en ms)
------------------------------------------------------------------------------------------------------------------*/
void InputReaderManager::set_sampling_Period(uint32_t value) {
    _samplingPeriod = value;
}


/*------------------------------------------------------------------------------------------------------------------
  Tare : requestTare()
  Lève le flag _tareRequested depuis la tâche HTTP (loop() / WiFi).
  Le flag est consommé de façon sûre dans _run() qui tourne dans sa propre tâche FreeRTOS.
  La variable est déclarée volatile pour forcer le compilateur à la relire en mémoire à chaque accès.
------------------------------------------------------------------------------------------------------------------*/
void InputReaderManager::requestTare() {
    _tareRequested = true;
    //Serial.println("[TARE] Demande de tare reçue — capture au prochain cycle.");
}


/*------------------------------------------------------------------------------------------------------------------
  Tare : resetTare()
  Remet tous les offsets à zéro et désactive la soustraction.
  Peut être appelé depuis la tâche HTTP à tout moment.
------------------------------------------------------------------------------------------------------------------*/
void InputReaderManager::resetTare() {
    _tareActive    = false;
    _tareRequested = false;
    memset(_offsets, 0, sizeof(_offsets));
    //Serial.println("[TARE] Réinitialisation — offsets remis à zéro.");
}


/*------------------------------------------------------------------------------------------------------------------
  taskWrapper() — Adaptateur C pour FreeRTOS (impossible de passer directement une méthode de classe)
------------------------------------------------------------------------------------------------------------------*/
void InputReaderManager::taskWrapper(void* pvParameters) {
    InputReaderManager *self = (InputReaderManager*) pvParameters;
    self->_run();
}


/*------------------------------------------------------------------------------------------------------------------
  _run() — Boucle principale de lecture (tâche FreeRTOS, priorité 2)

  Structure du buffer par cycle (5 doubles) :
      [timestamp] [jauge1] [jauge2] [jauge3] [jauge4]

  Logique de tare intégrée :
      1. On lit la valeur brute (raw_mV) de chaque voie.
      2. Si _tareRequested :
            → on stocke raw_mV comme offset de cette voie.
            → on soustrait immédiatement (résultat = 0.0 dès le premier cycle).
      3. Si _tareActive (tare précédent déjà capturé) :
            → on soustrait _offsets[voie] de chaque lecture.
      4. Après la boucle des voies : on confirme le tare (_tareActive = true, _tareRequested = false).
------------------------------------------------------------------------------------------------------------------*/
void InputReaderManager::_run() {

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(_samplingPeriod);

    double raw_mV      = 0.0;
    double value_in_mV = 0.0;
    double timestamp   = 0.0;

    while (true) {

        /* ── Timestamp ─────────────────────────────────────────────────────── */
        _currentBuffer[_index_buffer] = timestamp;
        timestamp += _samplingPeriod;
        _index_buffer++;
        if (_index_buffer >= BUFFER_SIZE) {
            _switchBuffer();
            _bufferReady  = true;
            _index_buffer = 0;
        }

        /* ── Snapshot local du flag tare (évite une double lecture volatile) ─ */
        bool doTareCapture = _tareRequested;

        /* ── Lecture de toutes les voies de tous les ADS1256 ────────────────── */
        for (InputDevice* device : _devices) {

            device->beginTransaction();

            for (int i = 0, idcapteur = 1; i <= 6; i += 2, idcapteur++) {

                /* 1. Lecture brute → conversion dans l'unité quantum (Volts) */
                device->setChannel(i, i + 1);
<<<<<<< HEAD
                delayMicroseconds(400);   // attendre la fin de la conversion après sync
=======
>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e
                raw_mV = device->readRaw() * device->getQuantum();

                /* 2. Capture de l'offset si tare demandé sur ce cycle */
                if (doTareCapture) {
                    _offsets[idcapteur - 1] = raw_mV;
                }

                /* 3. Soustraction de l'offset (actif dès la capture OU si tare déjà fait) */
                if (doTareCapture || _tareActive) {
                    value_in_mV = raw_mV - _offsets[idcapteur - 1];
                } else {
                    value_in_mV = raw_mV;
                }

                /* 4. Envoi moniteur série (format Teleplot) */
                Serial.printf(">Jauge %d : %.6f\n", idcapteur, value_in_mV);

                /* 5. Mise à jour interface Web */
                WifiManager::updateValue((float)value_in_mV);

                /* 6. Stockage dans le buffer actif */
                _currentBuffer[_index_buffer] = value_in_mV;
                _index_buffer++;
                if (_index_buffer >= BUFFER_SIZE) {
                    _switchBuffer();
                    _bufferReady  = true;
                    _index_buffer = 0;
                }
            }

            device->endTransaction();
        }

        /* ── Confirmation du tare après lecture complète des 4 voies ─────────
           On confirme ici (après la boucle) pour être sûr que les 4 offsets
           sont bien tous capturés avant d'activer la soustraction.          */
        if (doTareCapture) {
            _tareActive    = true;
            _tareRequested = false;
            Serial.printf("[TARE] Offsets capturés → J1:%.6f  J2:%.6f  J3:%.6f  J4:%.6f\n",
                          _offsets[0], _offsets[1], _offsets[2], _offsets[3]);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


/*------------------------------------------------------------------------------------------------------------------
  _switchBuffer() — Bascule entre BufferA et BufferB
------------------------------------------------------------------------------------------------------------------*/
void InputReaderManager::_switchBuffer() {
    if (_currentBuffer == _BufferA) {
        _fullBufferPtr = _BufferA;
        _currentBuffer = _BufferB;
    } else {
<<<<<<< HEAD
        _fullBufferPtr = _BufferB;
=======
        _fullBufferPtr = _BufferA;
>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e
        _currentBuffer = _BufferA;
    }
}


/*------------------------------------------------------------------------------------------------------------------
  isBufferReady() — Retourne true si un buffer est plein (et remet le flag à false)
------------------------------------------------------------------------------------------------------------------*/
bool InputReaderManager::isBufferReady() {
    if (_bufferReady) {
        _bufferReady = false;
        return true;
    }
    return false;
}

double* InputReaderManager::getBufferReady() { return _fullBufferPtr; }
int     InputReaderManager::getBufferSize()  { return BUFFER_SIZE;    }
