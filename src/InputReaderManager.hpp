/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim                           ************************************************************/
/********************************************* Fichier                          : InputReaderManager.hpp                 ************************************************************/
/********************************************* Description                      : Déclaration de la classe ADS1256 ***********************************************************/
/********************************************* Date de création(fr)             : 04/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      ************************************************************/


#ifndef INPUTREADERMANAGER_HPP
#define INPUTREADERMANAGER_HPP

#include <Arduino.h>
#include <vector>
#include <InputDevice.hpp>
#include <BUSspi.hpp>
/*******************************************Définition de la classe ***************************************************************************************/
class InputReaderManager {

    uint32_t _Sampling_Period=0;                                                        // Variable pour stocker le temps entre deux lectures
 
private:
    std::vector<ADS1256*> _devices;                                                     // On stocke les adresses de chaque cartes ici
    bool _isStarted = false;                                                            // savoir si la tâche FreeRTOS est déjà lancée ou non

public:

    InputReaderManager(uint32_t Sampling_Period);                                       // Constructeur 
    void begin();                                                                       // initialiser la carte ads connecté
    void addDevice(ADS1256* monADS);                                                    // ajoute de la carte ads à la liste _devices
    static void taskWrapper(void *pvParameters);                                        // freeRTOS en c++ pour lire les classes
    void task();                                                                        // une FCT pour lire les données à l'infini
    void set_Sampling_Period(uint32_t value);                                           // FCT pour changer la vitesse de lecture 

};

#endif