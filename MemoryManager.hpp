/**************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                       ********************************************/
/********************************************* Fichier                          : MemoryManager.hpp                 ********************************************/
/********************************************* Description                      : Manager les cartes mémoires SD *****************************************/
/********************************************* Date de création(fr)             : 03/03/2026                      *********************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      *********************************************/

/***********Classe et les attributs des cartes mémoires SD *******************************************************************************/




#ifndef MEMORYMANAGER_HPP
#define MEMORYMANAGER_HPP
#include <Arduino.h>
#include <vector>
#include <freertos/semphr.h>
#include <MemoryDevice.hpp>




class MemoryManager { 
private:
    static MemoryManager* _instance;
    std::vector<CardSD*> _devices;
    TaskHandle_t _sdTaskHandle = NULL;
    double* _dataTosave = nullptr;
    int _sizeTosave = 0;
    SemaphoreHandle_t _sdMutex = nullptr;
    MemoryManager();
    

public:
    
    static MemoryManager& getInstance();
    static void task(void *pvParameters);
    void addDevice(CardSD* myCard);
    void signalBufferReady(double* bufferWrite, int taille);
    void beginAll();
    void writeToAll(String row);
    void begin();
    void run();
    void setSdMutex(SemaphoreHandle_t m) { _sdMutex = m; }
    //void enableSafeEject(){_SafeEject = true;};                        // pour bloquer l'écriture
    /*void resetSafeEject(){
        _SafeEject = false;
        this->beginAll();                                             // ré-initialise la carte sd 
    }*/
    //bool isProtected() const {return _SafeEject;};
};

#endif