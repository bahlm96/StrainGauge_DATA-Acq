/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim                           ************************************************************/
/********************************************* Fichier                          : InputReaderManager.hpp           ************************************************************/
/********************************************* Description                      : Déclaration de la classe         ***********************************************************/
/********************************************* Date de création(fr)             : 04/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 21/04/2026                      ************************************************************/

/* Changelog :
    v0.2.0  --> Ajout du système de Tare / Zérotage par voie (requestTare, resetTare, getOffsets)
*/

#ifndef INPUTREADERMANAGER_HPP
#define INPUTREADERMANAGER_HPP

#include <Arduino.h>
#include <vector>
#include <ADS1256.hpp>
#include <BUSspi.hpp>
#include "InputDevice.hpp"


#define BUFFER_SIZE    2048
#define BUFFER_A       1
#define BUFFER_B       2
#define NB_CHANNELS    4       // Nombre de jauges lues par device (paires : AIN0/1, AIN2/3, AIN4/5, AIN6/7)


/**********************************************************************Définition de la classe *******************************************************************************/
class InputReaderManager {

private:
    std::vector<InputDevice*> _devices;                 // Liste des ADS1256 enregistrés
    uint32_t _samplingPeriod;

    /************************************************************Double buffer pour le stockage ***************************************************************************/
    double _BufferA[BUFFER_SIZE];
    double _BufferB[BUFFER_SIZE];
    double* _currentBuffer  = nullptr;
    int     _index_buffer   = 0;
    bool    _isStarted      = false;
    bool    _bufferReady;
    TaskHandle_t _taskHandle = NULL;
    double* _fullBufferPtr  = nullptr;


    /*************************************************************Tare — zérotage par voie ************************************************************************/
    /*  _offsets[i] : offset capturé sur la voie i (même unité que value_in_mV, c'est-à-dire en Volts selon getQuantum)
        _tareRequested : flag levé depuis la tâche HTTP (loop()), consommé dans _run()
        _tareActive    : true dès qu'au moins un tare a été effectué  */
    double          _offsets[NB_CHANNELS];              // Un offset par jauge
    volatile bool   _tareRequested;                     // Demande de capture (cross-task → volatile)
    volatile bool   _tareActive;                        // Soustraction active ou non


public:
    InputReaderManager(uint32_t samplingPeriod);

    void    begin();
    void    addDevice(InputDevice* monADS);
    static  void taskWrapper(void* pvParameters);

    void    set_sampling_Period(uint32_t value);

    double* getBufferReady();
    bool    isBufferReady();
    int     getBufferSize();

    /*************************************************************API Tare (appelée depuis WiFiManager) ***********************************************************/
    /* requestTare() : déclenche la capture des offsets au prochain cycle de lecture.
       resetTare()   : remet tous les offsets à zéro et désactive la soustraction.
       isTareActive(): indique si la soustraction d'offset est en cours.
       getOffsets()  : retourne un pointeur vers le tableau d'offsets (lecture seule, NB_CHANNELS éléments). */
    void    requestTare();
    void    resetTare();
    bool    isTareActive()  const { return _tareActive; }
    const double* getOffsets() const { return _offsets; }


private:
    void    _run();
    void    _switchBuffer();
    void    _getActiveBufferPtr();
};


#endif
