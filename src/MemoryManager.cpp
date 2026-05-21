/**************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                       ********************************************/
/********************************************* Fichier                          : MemoryManager.cpp                 ********************************************/
/********************************************* Description                      : Manager les cartes mémoires SD *****************************************/
/********************************************* Date de création(fr)             : 03/03/2026                      *********************************************/
/********************************************* Date de dernière modification    : 16/04/2026                      *********************************************/

/***********Son rôle est de manager plusieurs cartes mémoires SD *******************************************************************************/



#include <MemoryManager.hpp>
#include "WiFiManager.hpp"   
 
MemoryManager* MemoryManager::_instance = nullptr;
 
MemoryManager::MemoryManager() {}
 
MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance;
    return instance;
}
 
void MemoryManager::begin() {
<<<<<<< HEAD
    xTaskCreatePinnedToCore(MemoryManager::task, "SDTask", 4096, this, 1, &_sdTaskHandle, 1);
=======
    xTaskCreate(MemoryManager::task, "SDTask", 4096, this, 1, &_sdTaskHandle);
>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e
}
 
void MemoryManager::beginAll() {
    for (CardSD* card : _devices) {
        if (card->begin()) {
            Serial.println("Stockage : Carte SD initialisée avec succès.");
        } else {
            Serial.println("Stockage : Échec initialisation carte SD.");
        }
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
 
<<<<<<< HEAD
            /***********************Protection éjection sécurisée **********************/ 
=======
            // --- Protection éjection sécurisée ---
>>>>>>> 18d4e9bfce90d5fe4981888d494d41792f0cd29e
            if (WifiManager::isSafeEject()) {
                Serial.println("[SD] Éjection sécurisée active — écriture ignorée.");
                _dataTosave = nullptr;
                _sizeTosave = 0;
                continue;   // Ne pas écrire, attendre la prochaine notification
            }
 
            if (_dataTosave != nullptr && _sizeTosave > 0) {
                Serial.println("MemoryManager: Écriture du buffer sur SD...");
                for (CardSD* card : _devices) {
                    card->saveBuffer(_dataTosave, _sizeTosave);
                }
                Serial.println("MemoryManager: Fin d'écriture.");
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