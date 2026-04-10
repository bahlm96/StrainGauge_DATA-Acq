/**************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                       ********************************************/
/********************************************* Fichier                          : MemoryManager.cpp                 ********************************************/
/********************************************* Description                      : Manager les cartes mémoires SD *****************************************/
/********************************************* Date de création(fr)             : 03/03/2026                      *********************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      *********************************************/

/***********Son rôle est de manager plusieurs cartes mémoires SD *******************************************************************************/



#include <MemoryManager.hpp>


MemoryManager* MemoryManager::_instance = nullptr;

MemoryManager::MemoryManager(){    
}

// Dans MemoryManager.hpp
MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance; // Instance unique
    return instance;
}

/*
MemoryManager* MemoryManager::getInstance() {
    if (_instance == nullptr) {
        _instance = new MemoryManager();
    }
    return _instance;
}*/

void MemoryManager::begin() {
    xTaskCreate(MemoryManager::task, "SDTask", 4096, this, 1 , &_sdTaskHandle);
    
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

/*void MemoryManager::signalBufferReady(double* buffer, int taille) {
    _dataTosave = buffer;
    _sizeTosave = taille;
    if (_sdTaskHandle != NULL) {
        xTaskNotifyGive(_sdTaskHandle); // Réveille la tâche SD
    }
}    */

void MemoryManager::addDevice(CardSD* myCard) {                             // Ajoute la carte SD à la liste (tableau dynamique) du manager
    _devices.push_back(myCard);
}
// Cette méthode est appelée par le TaskManager pour donner l'ordre d'écrire
void MemoryManager::signalBufferReady(double* buffer, int taille) {
    if (buffer == nullptr) return;

    _dataTosave = buffer;
    _sizeTosave = taille;

    if (_sdTaskHandle != NULL) {
        // On réveille la tâche 'run' qui attend une notification
        xTaskNotifyGive(_sdTaskHandle); 
    }
}
           

void MemoryManager::task(void *pvParameters) {                              // Une tâche freeRTOS
((MemoryManager*)pvParameters)->run();
   
}   

void MemoryManager::run() {
    // Initialisation des cartes au démarrage de la tâche
    this->beginAll();

    while(1) {
        // La tâche s'endort ici et ne consomme 0% CPU tant qu'elle ne reçoit pas de notification
        // ulTaskNotifyTake(pdTRUE, portMAX_DELAY) attend le signal de signalBufferReady
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0) {


            /** Une petite protection pour la carte SD**
            if(_SafeEject){
                Serial.println("Mode protection de la carte SD");
                continue;
            }*/
            
            if (_dataTosave != nullptr && _sizeTosave > 0) {       // Si on a une adresse mémoire qu'est n'est pas vide et s'il y a vraiment quelquechose écrit (taille de la donée)
                Serial.println("MemoryManager: Écriture du buffer sur SD...");
                
                // On utilise la méthode optimisée saveBuffer de MemoryDevice
                for (CardSD* card : _devices) {
                    card->saveBuffer(_dataTosave, _sizeTosave);
                }
                
                Serial.println("MemoryManager: Fin d'écriture.");
                
                // Reset des pointeurs après écriture
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
