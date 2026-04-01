
/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : InputReaderManager              ************************************************************/
/********************************************* Description                      : Manager des cartes              ***********************************************************/
/********************************************* Date de création                 : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 30/03/2026                      ************************************************************/
/****************************************************************************************************************************************************************************/

#include "InputReaderManager.hpp"
#include "InputDevice.hpp"


/*****************************************Son rôle est de gérer plusieurs capteurs ou cartes simultanément et de
 s'assurer qu'ils lisent les données à une fréquence régulière sans bloquer le reste de l'ESP32, grâce à l'utilisation de FreeRTOS*****************************/

 
InputReaderManager::InputReaderManager(uint32_t samplingPeriod): 
    _samplingPeriod(samplingPeriod), _bufferReady(false), _index_buffer(0){
        _currentBuffer= _BufferA;
    } // Constructuer de la class InputReaderManager pour initialiser l'objet avec une période d'échantillonnage souhaité entre deux lectures.

void InputReaderManager::begin() {
    if (_isStarted) return;                                                                           // Si c'est déja lancé ne fait rien   
    for (InputDevice* device : _devices) {                                                              // faire appel à la configuration de chaque carte ajoutée                
        device->begin();
    };
    xTaskCreate(taskWrapper, "ReadyTask", 4096, this, 2, &_taskHandle);
    _isStarted = true;
    
}



double* InputReaderManager::getBufferReady() {
    _bufferReady = false; // La consigne : setté à False dès qu'on récupère le pointeur
    return _fullBufferPtr;
} 
void InputReaderManager::addDevice(InputDevice* monADS) {                                             // ajouter le pointeur de l'ADS dans un tableau dynamique
    _devices.push_back(monADS);                                                                        // ajoute à la fin de la liste _devices
}


void InputReaderManager::set_sampling_Period(uint32_t value){                                           // Modifier la vitesse de lecture 
    this->_samplingPeriod = value;
}


void InputReaderManager::taskWrapper(void *pvParameters)                                             // Traduire au freeRTOS les classes de langage C++ 
{
    InputReaderManager *self = (InputReaderManager*) pvParameters;

    self->task();                                                                                    // appel de la vraie fonction task()
}

void InputReaderManager::task() {                                                                   

  
        /*
                    Tous les this->_Sampling_Period ( ici this->_Sampling_Period = 10 ms )
                    On demande à tous nos devices de créeer une tache
                        Cette a pour rôle de lire TOUTES ses entrées (4, 8 ?), et de renvoyer les données associau InputdeviveManager
                        L'nput device manage doit ajouter ces données au buffer actuellement utilisé
                        Quand ce buffer est plein, il utilse l'autre buffer pour le stockage des données issues des retours des .
                        De plus en parallèle, l'inputdeviceManage, demande à "vider" le buffer plein dans la carte SD. ( Il ya donc une alterannce 'entre l'usage de deux buffer de même taille)

*/

    
    double value_in_mV;
    double timestamp= 0.0;

    while (true) {       

        // Remplissage du buffer actif
        _currentBuffer[_index_buffer]= timestamp;
        timestamp+= this->_samplingPeriod;
        _index_buffer ++;
        if (_index_buffer  >= BUFFER_SIZE) {
            _switchBuffer();
            _bufferReady = true;
            _index_buffer = 0;

        }

        for (InputDevice* device : _devices) {

            for (int i=0; i < 4; i+=2){
                if (_index_buffer  >= BUFFER_SIZE) {
                    _switchBuffer();
                    _bufferReady = true;
                    _index_buffer = 0;
                }
                device->setChannel(i, i+1);
                value_in_mV = device->readRaw() * device->getQuantum();// Lecture brute convertie

                _currentBuffer[_index_buffer]= value_in_mV;
                _index_buffer ++;


            }
            
            vTaskDelay(_samplingPeriod/portTICK_PERIOD_MS);
        }
    }
}



void InputReaderManager::_switchBuffer(){
    if (_currentBuffer ==_BufferA ) {
        _fullBufferPtr = _BufferA;
        _currentBuffer= _BufferB;
    } else {
        _fullBufferPtr = _BufferB;
        _currentBuffer =_BufferA;
    }
}

void InputReaderManager::deselect(int IdADS){
    digitalWrite(_devices[IdADS]->getCsPin(), LOW);
}

void InputReaderManager::select(int IdADS){
    digitalWrite(_devices[IdADS]->getCsPin(), HIGH);
}


bool InputReaderManager::isBufferReady() {

    if (_bufferReady){
        _bufferReady=false;
        return true;
    }
    return _bufferReady;
};

int InputReaderManager::getBufferSize() {
    return BUFFER_SIZE;
};                                         // Taille fixe du buffer plein 