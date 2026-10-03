/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : TaskManager.cpp                 ************************************************************/
/********************************************* Date de dernière modification    : 18/06/2026                      ************************************************************/

/* Changelog :
    v0.2.0  --> Découplage SD / WiFi :
                  - Suppression du poll (vTaskDelayUntil 50 ms + isBufferReady)
                  - La tâche est désormais bloquante : ulTaskNotifyTake(portMAX_DELAY)
    v0.3.0  --> Délégation de l'écriture SD au MemoryManager :
                  - TaskManager ne touche plus jamais SD directement
                  - Appelle _memoryManager->signalBufferReady(buf, size)
    v0.3.1  --> Correction pics :
                  - Retour au mode poll : vTaskDelay(50 ms) + isBufferReady()
                  - Suppression de setStorageTaskHandle / ulTaskNotifyTake
                  - Le buffer est toujours complet à la lecture (structure fixe garantie
                    par InputReaderManager v0.4.1)
                  - stride calculé depuis getActiveMask() pour filtrage CSV correct
*/

#include "TaskManager.hpp"
#include "MemoryManager.hpp"
#include "InputReaderManager.hpp"
#include "WiFiManager.hpp"

/* Constructeur */
TaskManager::TaskManager(InputReaderManager& inputReaderManager, MemoryManager& memoryManager)
    : _inputReaderManager(&inputReaderManager),
      _memoryManager(&memoryManager)
{}

/* begin() — Crée la tâche FreeRTOS */
void TaskManager::begin() {
    xTaskCreatePinnedToCore(taskWrapper, "StorageTask", 8192, this, 1, nullptr, 1);
}

/* taskWrapper() — Adaptateur C pour FreeRTOS */
void TaskManager::taskWrapper(void* pvParameters) {
    TaskManager* self = static_cast<TaskManager*>(pvParameters);
    self->run();
}

void TaskManager::run() {

    while (true) {

        /* Poll toutes les 50 ms — laisse le temps à _run() de finir d'écrire
           le cycle complet avant qu'on lise le buffer.                       */
        vTaskDelay(pdMS_TO_TICKS(50));

        if (!_inputReaderManager->isBufferReady()) {
            continue;
        }

        /* Snapshot du masque et du buffer */
        uint8_t mask = _inputReaderManager->getActiveMask();
        double* buf  = _inputReaderManager->getBufferReady();
        int     size = _inputReaderManager->getBufferSize();

        /* stride = 1 timestamp + 4 voies (buffer toujours complet).
           getActiveMask() indique au MemoryManager quelles colonnes
           écrire dans le CSV.                                         */
        int stride = 1 + __builtin_popcount(mask);
        if (!WifiManager::isRecordingSD() || stride <= 1 || buf == nullptr) {
            continue;
        }

        /* Délégation complète à MemoryManager :
           - prend le mutex SD
           - appelle CardSD::saveBuffer() sur chaque device enregistré
           - libère le mutex SD                                         */
        _memoryManager->signalBufferReady(buf, size);
    }
}