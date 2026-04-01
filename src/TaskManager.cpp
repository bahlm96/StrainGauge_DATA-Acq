#include "TaskManager.hpp"

TaskManager::TaskManager(InputReaderManager &inputReaderManager,MemoryManager &memoryManager):
    _inputReaderManager(&inputReaderManager), _memoryManager(&memoryManager){

}

void TaskManager::begin() {
    xTaskCreate(taskWrapper, "TaskManagerTask", 4096, this, 2, NULL);
}

void TaskManager::taskWrapper(void* pvParameters) {
    static_cast<TaskManager*>(pvParameters)->run();
}

void TaskManager::run() {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);
 
    
    while(true){

        

        if (_inputReaderManager->isBufferReady()){
            //Serial.println("On envois à la carte\n");
            //Serial.println(millis());
            
        }
        

        vTaskDelayUntil(&xLastWakeTime,xFrequency);
    }
}
// Appelé par InputReaderManager lorsqu'un buffer est plein
bool TaskManager::requestStorage(double* ptr, int size, int id) {
    //if (id == 1) _isBufferALocked = true;
    //else         _isBufferBLocked = true;

    //BufferRequest req = {ptr, size, id};
    //return (xQueueSend(_queue, &req, 0) == pdPASS); 
    return false;
}
