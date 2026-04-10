/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim                           ************************************************************/
/********************************************* Fichier                          : InputReaderManager.hpp                 ************************************************************/
/********************************************* Description                      : Déclaration de la classe ADS1256 ***********************************************************/
/********************************************* Date de création(fr)             : 04/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 12/03/2026                      ************************************************************/


#ifndef INPUTREADERMANAGER_HPP
#define INPUTREADERMANAGER_HPP

#include <Arduino.h>
#include <vector>
#include <ADS1256.hpp>
#include <BUSspi.hpp>
#include "InputDevice.hpp"



#define BUFFER_SIZE 2048
#define BUFFER_A 1
#define BUFFER_B 2


/**********************************************************************Définition de la classe *******************************************************************************/
class InputReaderManager {

    //uint32_t _Sampling_Period= 10;                                                    // Elle détermine à quelle fréquence ESP32 va demander une mesure à l'ADS1256.
 
private:
    std::vector<InputDevice*> _devices;                                                // On stocke les adresses de chaque cartes ici dans la variable appelée (_Devices)
    uint32_t _samplingPeriod = 100;

    
/****************************************************************Creation des buffer pour le stockage ************************************************************************/
    double _BufferA[BUFFER_SIZE];
    double _BufferB[BUFFER_SIZE];
    double* _currentBuffer = nullptr;    
    int _index_buffer = 0;
    bool _isStarted = false;                                                            // savoir si la tâche FreeRTOS est déjà lancée ou non
    bool _bufferReady;                                                                  // La consigne : setté à False au constructeur
    TaskHandle_t _taskHandle = NULL;
    double* _fullBufferPtr = nullptr;                                                   // Pour stocker l'adresse du buffer qui vient de se remplir
    

public:
    InputReaderManager(uint32_t samplingPeriod);                                        // Constructeur  +                                       
    void begin();                                                                       // initialiser la carte ads connecté
    void addDevice(InputDevice* monADS);                                               // ajoute de la carte ads à la liste _devices
    static void taskWrapper(void *pvParameters);                                        // freeRTOS en c++ pour lire les classes
   
    void set_sampling_Period(uint32_t value);                                           // FCT pour changer la vitesse de lecture 
    //void NextBuffer();
    void select(int IdADS);
    void deselect(int IdADS);
    void getActiveBufferPtr();
    double* getBufferReady();                                                           // Retourne le pointeur et repasse le flag à false
    bool isBufferReady();
    int getBufferSize();                                        // Taille fixe du buffer plein    

private:
    void _run();                                                                        // une FCT pour lire les données à l'infini
    void _switchBuffer();
    
};



#endif