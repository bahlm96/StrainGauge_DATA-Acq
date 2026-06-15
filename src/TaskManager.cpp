/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : TaskManager.cpp                 ************************************************************/
/********************************************* Date de dernière modification    : 15/06/2026                      ************************************************************/

/* Changelog :
    v0.2.0  --> Découplage SD / WiFi :
                  - Suppression du poll (vTaskDelayUntil 50 ms + isBufferReady)
                  - La tâche est désormais bloquante : ulTaskNotifyTake(portMAX_DELAY)
                    Elle se réveille uniquement quand InputReaderManager signale un buffer plein
                  - Suppression de BufferRequest (inutilisé)
    v0.3.0  --> Délégation de l'écriture SD au MemoryManager :
                  - TaskManager ne touche plus jamais SD directement (pas de SD.open/flush)
                  - Quand un buffer est prêt, TaskManager appelle
                    _memoryManager->signalBufferReady(buf, size)
                  - MemoryManager::run() prend le mutex SD, appelle CardSD::saveBuffer()
                    et gère les erreurs — TaskManager reste découplé du matériel SD
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
    _inputReaderManager->setStorageTaskHandle(xTaskGetCurrentTaskHandle());

    while (true) {

        /* Attente bloquante — réveil par xTaskNotifyGive() dans _switchBuffer() */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Snapshot du masque et du buffer au moment du réveil */
        uint8_t mask = _inputReaderManager->getActiveMask();
        double* buf  = _inputReaderManager->getBufferReady();
        int     size = _inputReaderManager->getBufferSize();

        /* stride = 1 timestamp + N valeurs capteurs actifs.
           Si mode "visu seule" (stride <= 1) ou SD désactivé → on ignore. */
        int stride = 1 + __builtin_popcount(mask);
        if (!WifiManager::isRecordingSD() || stride <= 1 || buf == nullptr) {
            continue;
        }

        /* Délégation complète à MemoryManager :
           - prend le mutex SD
           - appelle CardSD::saveBuffer() sur chaque device enregistré
           - libère le mutex SD                                            */
        _memoryManager->signalBufferReady(buf, size);
    }
}