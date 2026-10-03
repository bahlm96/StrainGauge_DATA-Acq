#ifndef TASKMANAGER_HPP
#define TASKMANAGER_HPP

/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : TaskManager.hpp                 ************************************************************/
/********************************************* Date de dernière modification    : 18/06/2026                      ************************************************************/

/* Changelog :
    v0.2.0  --> Découplage SD / WiFi :
                  - Suppression du poll (vTaskDelayUntil 50 ms + isBufferReady)
                  - La tâche est désormais bloquante : ulTaskNotifyTake(portMAX_DELAY)
    v0.3.0  --> Délégation de l'écriture SD au MemoryManager
    v0.3.1  --> Correction pics :
                  - Retour au mode poll isBufferReady() (suppression ulTaskNotifyTake)
                  - Suppression de setStorageTaskHandle (plus utilisé)
*/

#include <Arduino.h>
#include "MemoryManager.hpp"
#include "InputReaderManager.hpp"

class MemoryManager;

class TaskManager {
private:
    InputReaderManager* _inputReaderManager;
    MemoryManager*      _memoryManager;

    static void taskWrapper(void* pvParameters);
    void        run();

public:
    TaskManager(InputReaderManager& inputReaderManager, MemoryManager& memoryManager);
    void begin();
};

#endif