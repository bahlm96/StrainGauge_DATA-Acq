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
#include <mutex>
#include <MemoryDevice.hpp>




class MemoryManager { 
private:
    static MemoryManager* _instance;
    std::vector<CardSD*> _devices;                                    // La liste des cartes SD connectées
    TaskHandle_t _sdTaskHandle = NULL;
    volatile bool _SafeEject = false;                                 // Flag pour stopper l'écriture
    double* _dataTosave = nullptr;
    int _sizeTosave = 0;
    MemoryManager();                                            // Constructeur privé pour faire un singleton
    

public:
    
    static MemoryManager& getInstance();                              // communication avec le Manager 
    static void task(void *pvParameters);                                    // une tâche FreeRTOS pour la carte SD
    void addDevice(CardSD* myCard);                                   // Enregistrer une nouvelle carte SD
    void signalBufferReady(double* bufferWrite, int taille);               // Cette méthode qui sera appellé par le TASKMANAGER
    void beginAll();                                                  // Allumer toutes les cartes SD de la liste
    void writeToAll(String row);                                      // Écrire une ligne sur toutes les cartes
    void begin();                                                     // Initialisation de la carte SD
    void run();
    void enableSafeEject(){_SafeEject = true;};                        // pour bloquer l'écriture
    void resetSafeEject(){
        _SafeEject = false;
        this->beginAll();                                             // ré-initialise la carte sd 
    }
    bool isProtected() const {return _SafeEject;};
};

#endif