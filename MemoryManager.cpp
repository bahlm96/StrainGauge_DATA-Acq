/**************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                       ********************************************/
/********************************************* Fichier                          : MemoryManager.cpp                 *******************************************/
/********************************************* Date de dernière modification    : 02/06/2026                      *********************************************/

#include <MemoryManager.hpp>
#include "WiFiManager.hpp"

MemoryManager* MemoryManager::_instance = nullptr;

MemoryManager::MemoryManager() {}

MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance;
    return instance;
}

void MemoryManager::begin() {
    // Exécution forcée sur le Cœur 1 pour ne pas perturber le Wi-Fi (Cœur 0)
    xTaskCreatePinnedToCore(MemoryManager::task, "SDTask", 4096, this, 1, &_sdTaskHandle, 1);
}

void MemoryManager::beginAll() {
    uint8_t mask = WifiManager::getSensorMask();
    for (CardSD* card : _devices) {
        card->begin(mask);
    }
}

void MemoryManager::signalBufferReady(double* buffer, int taille) {
    _dataTosave = buffer;
    _sizeTosave = taille;
    if (_sdTaskHandle != NULL) {
        xTaskNotifyGive(_sdTaskHandle);
    }
}

void MemoryManager::addDevice(CardSD* myCard) {
    _devices.push_back(myCard);
}

void MemoryManager::task(void* pvParameters) {
    ((MemoryManager*)pvParameters)->run();
}

void MemoryManager::run() {
    this->beginAll();

    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0) {

            if (WifiManager::isSafeEject()) {
                _dataTosave = nullptr;
                _sizeTosave = 0;
                continue;
            }

            if (_dataTosave != nullptr && _sizeTosave > 0) {

                bool gotMutex = false;
                if (_sdMutex) {
                    gotMutex = (xSemaphoreTake(_sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE);
                }

                if (gotMutex) {
                    for (CardSD* card : _devices) {
                        // C'est ici que votre fonction saveBuffer reçoit l'adresse du bloc complet de données stables
                        card->saveBuffer(_dataTosave, _sizeTosave);
                    }
                    if (_sdMutex) xSemaphoreGive(_sdMutex);
                }

                _dataTosave = nullptr;
                _sizeTosave = 0;
            }
        }
    }
}

void MemoryManager::writeToAll(String row) {
    for (CardSD* card : _devices) {
        card->saveRow(row);
    }
}