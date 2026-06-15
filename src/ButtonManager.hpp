/******************************************************************************/
/* Auteur  : Halim BALA                                                       */
/* Fichier : ButtonManager.hpp                                                */
/* Date    : 12/06/2026                                                       */
/******************************************************************************/

#ifndef BUTTONMANAGER_HPP
#define BUTTONMANAGER_HPP

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/* GPIO boutons */
#define BTN_NEXT     15    //* changer jauge        
#define BTN_VALID    21    //* ON/OFF + valider       
#define BTN_PREV     22    //* suspend/reprendre     
#define BTN_STOP     34    //* arret                 

/* Timing */
#define BTN_DEBOUNCE_MS      20
#define BTN_LONG_PRESS_MS    800
#define CONFIRM_TIMEOUT_MS   3000
#define LCD_REFRESH_MS       500
#define LCD_SCROLL_MS        2000

/* Etats */
enum class BtnState : uint8_t {
    WELCOME   = 0,
    SELECT    = 1,
    RUNNING   = 2,
    SUSPENDED = 3,
    CONFIRM   = 4,
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
    void _showSelect();
    void _showRunning();
    void _showSuspended();
    void _showConfirm();

    int     _activeCount() const;
    uint8_t _buildMask()   const;
    void    _startRunning();
    void    _stopAcq();

    /* Etat interne */
    volatile BtnState _state;
    int        _cursor;
    bool       _active[4];
    uint8_t    _scrollPage;
    TickType_t _lastScrollTick;
    TickType_t _lastLcdTick;
    TickType_t _confirmStart;

    BtnDebounce _dbNext;
    BtnDebounce _dbValid;
    BtnDebounce _dbPrev;
    BtnDebounce _dbStop;
};

#endif