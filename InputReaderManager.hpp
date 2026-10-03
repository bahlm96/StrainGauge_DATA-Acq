/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : InputReaderManager.hpp           ************************************************************/
/********************************************* Description                      : Déclaration de la classe         ***********************************************************/
/********************************************* Date de création(fr)             : 04/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 18/06/2026                      ************************************************************/

/* Changelog :
    v0.2.0  --> Tare / Zérotage
    v0.3.0  --> _bufferReady + isBufferReady() poll
    v0.4.0  --> Sélection capteurs + mode SD + LED
    v0.4.2  --> _lastValues[4] pour Serial/WiFi hors boucle SPI
*/

#ifndef INPUTREADERMANAGER_HPP
#define INPUTREADERMANAGER_HPP

#include <Arduino.h>
#include <vector>
#include <ADS1256.hpp>
#include <BUSspi.hpp>
#include "InputDevice.hpp"

#define BUFFER_SIZE    2048
#define NB_CHANNELS    4

class InputReaderManager {

private:
    std::vector<InputDevice*> _devices;
    uint32_t _samplingPeriod;

    double       _BufferA[BUFFER_SIZE];
    double       _BufferB[BUFFER_SIZE];
    double*      _currentBuffer  = nullptr;
    int          _index_buffer   = 0;
    bool         _isStarted      = false;
    bool         _bufferReady    = false;
    TaskHandle_t _taskHandle     = NULL;
    double*      _fullBufferPtr  = nullptr;

    double        _offsets[NB_CHANNELS];
    volatile bool _tareRequested;
    volatile bool _tareActive;

    uint8_t       _activeMask    = 0x0F;
    bool          _recordToSD    = true;

    bool          _ledState      = false;
    uint16_t      _ledToggleCount = 0;

    /* Valeurs mémorisées pendant la boucle SPI,
       envoyées à Serial/WiFi APRÈS endTransaction() */
    double        _lastValues[NB_CHANNELS];

public:
    InputReaderManager(uint32_t samplingPeriod);

    void    begin();
    void    addDevice(InputDevice* monADS);
    static  void taskWrapper(void* pvParameters);
    void    set_sampling_Period(uint32_t value);
    void    setAllSPS(uint8_t drate);

    double* getBufferReady();
    bool    isBufferReady();
    int     getBufferSize();

    void    requestTare();
    void    resetTare();
    bool    isTareActive()     const { return _tareActive; }
    const double* getOffsets() const { return _offsets;    }

    uint8_t getActiveMask()        const { return _activeMask; }
    int     getActiveSensorCount() const;

private:
    void _run();
    void _switchBuffer();
};

#endif