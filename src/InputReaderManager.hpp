/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim                           ************************************************************/
/********************************************* Fichier                          : InputReaderManager.hpp           ************************************************************/
/********************************************* Description                      : Déclaration de la classe         ***********************************************************/
/********************************************* Date de création(fr)             : 04/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 05/06/2026                      ************************************************************/

/* Changelog :
    v0.2.0  --> Ajout du système de Tare / Zérotage par voie (requestTare, resetTare, getOffsets)
    v0.3.0  --> Découplage SD / WiFi :
                  - Suppression de isBufferReady() / _bufferReady (poll toutes les 50 ms)
                  - Ajout de _storageTaskHandle : _switchBuffer() notifie directement
                    le TaskManager via xTaskNotifyGive() → zéro délai, zéro CPU inutile
                  - setStorageTaskHandle() appelé par TaskManager au démarrage de sa tâche
    v0.4.0  --> Sélection capteurs + mode SD :
                  - Membres _activeMask (uint8_t) et _recordToSD (bool) ajoutés
                  - getActiveMask() : retourne le masque courant (pour TaskManager -> en-tête CSV)
                  - getActiveSensorCount() : nombre de bits actifs dans le masque
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
    double* _currentBuffer      = nullptr;
    int     _index_buffer       = 0;
    bool    _isStarted          = false;
    TaskHandle_t _taskHandle    = NULL;
    double* _fullBufferPtr      = nullptr;

    /* Handle de la tâche TaskManager — notifiée directement quand un buffer est plein.
       Remplace le flag _bufferReady + poll toutes les 50 ms. */
    TaskHandle_t _storageTaskHandle = NULL;


    /*************************************************************Tare — zérotage par voie ************************************************************************/
    /*  _offsets[i] : offset capturé sur la voie i (même unité que value_in_mV, c'est-à-dire en Volts selon getQuantum)
        _tareRequested : flag levé depuis la tâche HTTP (loop()), consommé dans _run()
        _tareActive    : true dès qu'au moins un tare a été effectué  */
    double          _offsets[NB_CHANNELS];              // Un offset par jauge
    volatile bool   _tareRequested;                     // Demande de capture (cross-task → volatile)
    volatile bool   _tareActive;                        // Soustraction active ou non

    /*************************************************************Sélection capteurs + mode SD ********************************************************************/
    /*  _activeMask  : bits 0-3 → capteurs J1-J4 actifs (ex: 0b0001 = J1 seul, 0x0F = tous)
                       Lu depuis WifiManager::getSensorMask() à chaque cycle de _run().
        _recordToSD  : true si mode "both" ou "sd seule" — false en mode "visu seule".
                       Lu depuis WifiManager::isRecordingSD() à chaque cycle de _run().    */
    uint8_t         _activeMask;                        // Masque des capteurs actifs
    bool            _recordToSD;                        // Enregistrement SD activé ou non

    /* ── LED acquisition (GPIO LED_ACQ_PIN) ──────────────────────────── */
    bool            _ledState       = false;            // Niveau courant de la LED
    uint16_t        _ledToggleCount = 0;                // Compteur de cycles pour le toggle


public:
    InputReaderManager(uint32_t samplingPeriod);

    void    begin();
    void    addDevice(InputDevice* monADS);
    static  void taskWrapper(void* pvParameters);

    void    set_sampling_Period(uint32_t value);

    /* setAllSPS() : applique un code registre DRATE à tous les ADS1256 enregistrés.
       Appelé depuis WiFiManager via la route POST /setSPS.                          */
    void    setAllSPS(uint8_t drate);

    /* setStorageTaskHandle() — enregistre le handle de la tâche TaskManager.
       Doit être appelé depuis TaskManager::run() avant la boucle principale,
       via xTaskGetCurrentTaskHandle().                                        */
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