/**************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                       ********************************************/
/********************************************* Fichier                          : MemoryManager.cpp                 ********************************************/
/********************************************* Description                      : Manager les cartes mémoires SD *****************************************/
/********************************************* Date de création(fr)             : 03/03/2026                      *********************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      *********************************************/

/***********Son rôle est de manager plusieurs cartes mémoires SD *******************************************************************************/



#include <MemoryManager.hpp>

void MemoryManager::begin() {
    
}

void MemoryManager::beginAll() {                                            // FCT pour parcourire la liste de toutes les cartes SD enregistrées
    for (CardSD* card : _devices) {
        if (card->begin()) {                                                // Allumer la carte 
            Serial.println("Stockage : Carte SD initialisée avec succès.");
        } else {
            Serial.println("Stockage : Échec initialisation carte SD.");
        }
    }
}

void MemoryManager::addDevice(CardSD* myCard) {                             // Ajoute la carte SD à la liste (tableau dynamique) du manager
    _devices.push_back(myCard);
}

void MemoryManager::writeToAll(String row) {
    for (CardSD* card : _devices) {                                         // pour chaque carte mémoire enregistrer dans la liste
        card->saveRow(row);                                                 // enregistrer la ligne de données (row)
    }
}


void MemoryManager::task(void *pvParameters) {                              // Une tâche freeRTOS

    while(1){
        
    }
}