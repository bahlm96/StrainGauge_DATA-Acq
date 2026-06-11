/**************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                       ********************************************/
/********************************************* Fichier                          : ButtonManager.hpp                 ********************************************/
/********************************************* Description                      : Gestion 4 boutons + menu LCD      ********************************************/
/********************************************* Date de création                 : 11/06/2026                       *********************************************/

#ifndef BUTTONMANAGER_HPP
#define BUTTONMANAGER_HPP

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/*Broches*/
#define BTN_PREV     22
#define BTN_SUSPEND  34
#define BTN_STOP     21
#define BTN_NEXT     15

/* Timing anti-rebond / appui long*/
#define DEBOUNCE_MS   50
#define LONG_PRESS_MS 800  // en ms

class ButtonManager {
public:
    /* États — enum class pour éviter tout conflit de noms avec le SDK ESP32 */
    enum class State : uint8_t {
        MENU = 0,
        RUNNING,
        PAUSED
    };

    ButtonManager();
    void begin();

private:
    State    _state;
    uint8_t  _cursor;
    bool     _active[4];

    uint32_t _lastPress[4];
    bool     _pressed[4];

    uint32_t _nextPressStart;
    bool     _nextHeld;

    static void taskEntry(void* pv);
    void        run();

    bool _read(uint8_t pin, uint8_t idx);

    void _onPrev();
    void _onSuspend();
    void _onStop();
    void _onNext();
    void _onNextLong();

    void _startAcq();
    void _stopAcq();

    void _drawMenu();
    void _drawRunning();
    void _drawPaused();

    uint8_t _buildMask() const;
};

#endif