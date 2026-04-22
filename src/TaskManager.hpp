#ifndef TASKMANAGER_HPP
#define TASKMANAGER_HPP

#include <Arduino.h>
#include "MemoryManager.hpp"
#include "InputReaderManager.hpp"

class MemoryManager;
// Structure pour transporter les infos du buffer plein
struct BufferRequest {
    double* bufferPtr;
    int size;
    int bufferId; // 1 pour BufferA, 2 pour BufferB
};

class TaskManager {
private:
    InputReaderManager *_inputReaderManager;
    MemoryManager *_memoryManager;

    static void taskWrapper(void* pvParameters);
    void run();

public:
    TaskManager(InputReaderManager& inputReaderManager, MemoryManager& memoryManager);
    void begin();

private:
    //bool requestStorage(double* ptr, int size, int id);    
};

#endif