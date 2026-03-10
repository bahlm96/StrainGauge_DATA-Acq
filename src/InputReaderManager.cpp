
/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : InputReaderManager              ************************************************************/
/********************************************* Description                      : Manager des cartes              ***********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      ************************************************************/
/****************************************************************************************************************************************************************************/

#include <InputReaderManager.hpp>



/*****************************************Son rôle est de gérer plusieurs capteurs ou cartes simultanément et de
 s'assurer qu'ils lisent les données à une fréquence régulière sans bloquer le reste de l'ESP32, grâce à l'utilisation de FreeRTOS*****************************/

 
InputReaderManager::InputReaderManager(uint32_t Sampling_Period): _Sampling_Period(Sampling_Period){} // Constructuer de la class InputReaderManager pour initialiser l'objet avec une période d'échantillonnage souhaité entre deux lectures.

void InputReaderManager::begin() {
    if (_isStarted) return;                                                                           // Si c'est déja lancé ne fait rien   
    for (ADS1256* dev : _devices) {                                                                   // faire appel à la configuration de chaque carte ajoutée                
        dev->begin();
    }
    _isStarted = true;
    xTaskCreate(taskWrapper, "InputReaderManager", 2048, this, 1, NULL);                              // une tâche freeRTOS
}



void InputReaderManager::addDevice(ADS1256* monADS) {                                                // ajouter le pointeur de l'ADS dans un tableau dynamique
    _devices.push_back(monADS);
}

void InputReaderManager::set_Sampling_Period(uint32_t value){                                        // Modifier la vitesse de lecture 
    this->_Sampling_Period = value;
}


void InputReaderManager::taskWrapper(void *pvParameters)                                             // Traduire au freeRTOS les classes de langage C++ 
{
    InputReaderManager *self = (InputReaderManager*) pvParameters;

    self->task();                                                                                    // appel de la vraie fonction
}


void InputReaderManager::task() {                                                                   
    uint32_t counter =0;                                                                             // initialiser le compteur 
    Serial.println("Tache démarrée");
    
    while (1) {                                                                                      // une boucle qui tourne à l'infinie
        counter++;
        if (counter >= this->_Sampling_Period){                                                      // En va demander à nos cartes d'acquistion de lire les données  
        }

        vTaskDelay(1 / portTICK_PERIOD_MS);                                                          // Délai FreeRTOS (1ms)
    }
}

