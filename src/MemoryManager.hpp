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
    std::vector<CardSD*> _devices;                                    // La liste des cartes SD connectées

    MemoryManager() {}                                                // Constructeur privé pour faire un singleton

public:
    static MemoryManager* getInstance();                              // communication avec le Manager 
    void task(void *pvParameters);                                    // une tâche FreeRTOS pour la carte SD
    void addDevice(CardSD* myCard);                                   // Enregistrer une nouvelle carte SD
    void beginAll();                                                  // Allumer toutes les cartes SD de la liste
    void writeToAll(String row);                                      // Écrire une ligne sur toutes les cartes
    void begin();                                                     // Initialisation de la carte SD
};

#endif