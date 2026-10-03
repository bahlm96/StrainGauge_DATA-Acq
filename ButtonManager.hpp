/******************************************************************************/
/* Auteur  : Halim BALA                                                       */
/* Fichier : ButtonManager.hpp                                                */
/* Date    : 22/06/2026                                                       */
/******************************************************************************/

#ifndef BUTTONMANAGER_HPP
#define BUTTONMANAGER_HPP

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/* GPIO boutons */
#define BTN_SCROLL   15    //* WELCOME : court = jauge suivante, long = ON/OFF jauge
#define BTN_MEASURE  21    //* court = start/stop mesure, long = suspend/reprise
#define BTN_EJECT    13    //* court = eject SD, long = reset eject (LED bleue = LED_SAFE_EJECT, GPIO2, definie dans WiFiManager.hpp)
                            //* (deplace de 22 vers 13 : GPIO22 est desormais LED_ACQ, voir WiFiManager.hpp)

/* Timing */
#define BTN_DEBOUNCE_MS      20
#define BTN_LONG_PRESS_MS    800
#define LCD_REFRESH_MS       500
#define LCD_SCROLL_MS        2000

/* Etats */
enum class BtnState : uint8_t {
    WELCOME   = 0,
    RUNNING   = 1,
    SUSPENDED = 2,
};

/* Evenements */
enum class BtnEvent : uint8_t {
    NONE        = 0,
    SHORT_PRESS = 1,
    LONG_PRESS  = 2,
};

/* Debounce par bouton */
struct BtnDebounce {
    int        lastRaw;
    int        stableLevel;
    TickType_t debounceStart;
    TickType_t pressStart;
    bool       pressHandled;
};


class ButtonManager {
public:
    ButtonManager();
    void     begin();
    BtnState getState() const { return _state; }

private:
    static void _taskWrapper(void* pv);
    void        _run();

    BtnEvent _read(BtnDebounce& db, uint8_t pin);

    void _showWelcome();
    void _showRunning();
    void _showSuspended();

    int     _activeCount() const;
    uint8_t _buildMask()   const;
    void    _startRunning();
    void    _stopAcq();

    /* Gestion bouton eject (independante de la machine a etats principale) */
    void    _handleEjectButton(BtnEvent evt);

    /* Etat interne */
    volatile BtnState _state;
    int        _cursor;
    bool       _active[4];
    uint8_t    _scrollPage;
    TickType_t _lastScrollTick;
    TickType_t _lastLcdTick;

    BtnDebounce _dbScroll;
    BtnDebounce _dbMeasure;
    BtnDebounce _dbEject;
};

#endif