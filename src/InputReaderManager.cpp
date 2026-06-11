/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : InputReaderManager.cpp           ************************************************************/
/********************************************* Description                      : Manager des cartes              ***********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 05/06/2026                      ************************************************************/



#include "InputReaderManager.hpp"
#include "InputDevice.hpp"
#include "WiFiManager.hpp"


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
    xTaskCreatePinnedToCore(taskWrapper, "ReadyTask", 8192, this, 2, &_taskHandle, 1);
    _isStarted = true;
}


/* addDevice() — Enregistre un ADS1256 et l'initialise immédiatement */
void InputReaderManager::addDevice(InputDevice* device) {
    _devices.push_back(device);
    device->begin();
}


/* set_sampling_Period() — Modifie la période FreeRTOS (en ms) */
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

        /*Timestamp**** */
    
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

        /* Lecture de toutes les voies *****/
        for (InputDevice* device : _devices) {

            device->beginTransaction();

            for (int i = 0, idcapteur = 1; i <= 6; i += 2, idcapteur++) {

                bool capteurActif = (_activeMask & (1 << (idcapteur - 1))) != 0;

                device->setChannel(i, i + 1);
                delayMicroseconds(400);
                raw_mV = device->readRaw() * device->getQuantum();

                
                if (doTareCapture) {
                    _offsets[idcapteur - 1] = raw_mV;
                }

                
                if (doTareCapture || _tareActive) {                 /*  Soustraction de l'offset */
                    value_in_mV = raw_mV - _offsets[idcapteur - 1];
                } else {
                    value_in_mV = raw_mV;
                }
                //Serial.printf(">Jauge %d : %.6f\n", idcapteur, value_in_mV);
                /*  Mise à jour interface Web — uniquement si capteur actif */
                if (capteurActif) {
                    WifiManager::updateGauge(idcapteur - 1, (float)value_in_mV);
                }

                /* Stockage SD — uniquement si capteur actif & mode SD activé */
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
