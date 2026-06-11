/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim                           ************************************************************/
/********************************************* Fichier                          : InputReaderManager.hpp           ************************************************************/
/********************************************* Description                      : Déclaration de la classe         ***********************************************************/
/********************************************* Date de création(fr)             : 04/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 05/06/2026                      ************************************************************/



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
    double* _currentBuffer      = nullptr;
    int     _index_buffer       = 0;
    bool    _isStarted          = false;
    TaskHandle_t _taskHandle    = NULL;
    double* _fullBufferPtr      = nullptr;

    /* Handle de la tâche TaskManager — notifiée directement quand un buffer est plein.
       Remplace le flag _bufferReady + poll toutes les 50 ms. */
    TaskHandle_t _storageTaskHandle = NULL;


    /*************************************************************Tare — zérotage par voie ************************************************************************/
    double          _offsets[NB_CHANNELS];              // Un offset par jauge
    volatile bool   _tareRequested;                     // Demande de capture (cross-task → volatile)
    volatile bool   _tareActive;                        // Soustraction active ou non

    /*************************************************************Sélection capteurs + mode SD ********************************************************************/

    uint8_t         _activeMask;                        // Masque des capteurs actifs
    bool            _recordToSD;                        // Enregistrement SD activé ou non


public:
    InputReaderManager(uint32_t samplingPeriod);

    void    begin();
    void    addDevice(InputDevice* monADS);
    static  void taskWrapper(void* pvParameters);

    void    set_sampling_Period(uint32_t value);

    /* setAllSPS() : applique un code registre DRATE à tous les ADS1256 enregistrés.                         */
    void    setAllSPS(uint8_t drate);

    /* setStorageTaskHandle() — enregistre le handle de la tâche TaskManager                                       */
    void    setStorageTaskHandle(TaskHandle_t h) { _storageTaskHandle = h; }

    double* getBufferReady();
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

    /*************************************************************API Selection capteurs ***************************************************************************/
    uint8_t getActiveMask()        const;
    int     getActiveSensorCount() const;


private:
    void    _run();
    void    _switchBuffer();
    void    _getActiveBufferPtr();
};


#endif
