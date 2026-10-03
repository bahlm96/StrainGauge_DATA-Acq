/******************************************************************************/
/* Auteur  : Halim BALA                                                       */
/* Fichier : ButtonManager.cpp                                                */
/* Date    : 22/06/2026                                                       */
/******************************************************************************/

#include "ButtonManager.hpp"
#include "WiFiManager.hpp"

/* Init debounce */
static BtnDebounce dbInit(uint8_t pin) {
    BtnDebounce d;
    d.lastRaw       = digitalRead(pin);
    d.stableLevel   = d.lastRaw;
    d.debounceStart = 0;
    d.pressStart    = 0;
    d.pressHandled  = false;
    return d;
}

/* Constructeur */
ButtonManager::ButtonManager()
    : _state(BtnState::WELCOME),
      _cursor(0),
      _active{true, true, true, true},
      _scrollPage(0),
      _lastScrollTick(0),
      _lastLcdTick(0),
      _dbScroll (dbInit(BTN_SCROLL)),
      _dbMeasure(dbInit(BTN_MEASURE)),
      _dbEject  (dbInit(BTN_EJECT))
{}

/* begin()*/
void ButtonManager::begin() {
    /* GPIO 15, 21, 13 */
    pinMode(BTN_SCROLL,  INPUT_PULLUP);
    pinMode(BTN_MEASURE, INPUT_PULLUP);
    pinMode(BTN_EJECT,   INPUT_PULLUP);

    /* LED bleue d'ejection (GPIO2, definie dans WiFiManager.hpp) */
    pinMode(LED_SAFE_EJECT, OUTPUT);
    digitalWrite(LED_SAFE_EJECT, WifiManager::isSafeEject() ? HIGH : LOW);

    /* Relecture apres pinMode */
    _dbScroll  = dbInit(BTN_SCROLL);
    _dbMeasure = dbInit(BTN_MEASURE);
    _dbEject   = dbInit(BTN_EJECT);

    _showWelcome();

    xTaskCreatePinnedToCore(_taskWrapper, "BtnTask", 2048, this, 1, nullptr, 1);
    Serial.println("[BTN] demarrage : SCROLL=15 MEASURE=21 EJECT=13");
}

void ButtonManager::_taskWrapper(void* pv) {
    static_cast<ButtonManager*>(pv)->_run();
}

/*  _run() */
void ButtonManager::_run() {
    const TickType_t loopTick = pdMS_TO_TICKS(10);
    TickType_t xLastWake = xTaskGetTickCount();

    while (true) {

        BtnEvent evtScroll  = _read(_dbScroll,  BTN_SCROLL);
        BtnEvent evtMeasure = _read(_dbMeasure, BTN_MEASURE);
        BtnEvent evtEject   = _read(_dbEject,   BTN_EJECT);

        /* Le bouton EJECT est independant de la machine a etats principale : */
        /* il agit a tout moment, quel que soit l'etat de la mesure.          */
        _handleEjectButton(evtEject);

        switch (_state) {

            /* WELCOME */
            /*
               SCROLL court : curseur suivant J1->J2->J3->J4->J1
               SCROLL long  : ON / OFF la jauge pointee par le curseur
               MEASURE court: lance la mesure -> RUNNING
            */
            case BtnState::WELCOME:
                if (evtScroll == BtnEvent::SHORT_PRESS) {
                    _cursor = (_cursor + 1) % 4;
                    Serial.printf("[BTN] WELCOME cursor J%d\n", _cursor + 1);
                    _showWelcome();
                }
                else if (evtScroll == BtnEvent::LONG_PRESS) {
                    _active[_cursor] = !_active[_cursor];
                    Serial.printf("[BTN] WELCOME J%d %s\n",
                                  _cursor + 1, _active[_cursor] ? "ON" : "OFF");
                    _showWelcome();
                }
                else if (evtMeasure == BtnEvent::SHORT_PRESS) {
                    if (_activeCount() == 0) _active[0] = true;
                    Serial.println("[BTN] WELCOME -> RUNNING (MEASURE court)");
                    _startRunning();
                }
                break;

            /* RUNNING */
            /*
               MEASURE court : arret definitif -> WELCOME
               MEASURE long  : suspend -> SUSPENDED
            */
            case BtnState::RUNNING:
                if (evtMeasure == BtnEvent::SHORT_PRESS) {
                    Serial.println("[BTN] RUNNING -> WELCOME (MEASURE court)");
                    _stopAcq();
                }
                else if (evtMeasure == BtnEvent::LONG_PRESS) {
                    WifiManager::setRecordingSD(false);
                    _state = BtnState::SUSPENDED;
                    Serial.println("[BTN] RUNNING -> SUSPENDED (MEASURE long)");
                    _showSuspended();
                }
                break;

            /* SUSPENDED */
            /*
               MEASURE court : arret definitif -> WELCOME
               MEASURE long  : reprise -> RUNNING
            */
            case BtnState::SUSPENDED:
                if (evtMeasure == BtnEvent::SHORT_PRESS) {
                    Serial.println("[BTN] SUSPENDED -> WELCOME (MEASURE court)");
                    _stopAcq();
                }
                else if (evtMeasure == BtnEvent::LONG_PRESS) {
                    WifiManager::setRecordingSD(true);
                    _state = BtnState::RUNNING;
                    _lastLcdTick    = xTaskGetTickCount();
                    _lastScrollTick = xTaskGetTickCount();
                    Serial.println("[BTN] SUSPENDED -> RUNNING (MEASURE long)");
                    _showRunning();
                }
                break;
        }

        /* Actions periodiques en RUNNING */
        if (_state == BtnState::RUNNING) {
            TickType_t now = xTaskGetTickCount();
            if ((now - _lastLcdTick) >= pdMS_TO_TICKS(LCD_REFRESH_MS)) {
                _lastLcdTick = now;
                _showRunning();
            }
            if (_activeCount() > 2 &&
                (now - _lastScrollTick) >= pdMS_TO_TICKS(LCD_SCROLL_MS)) {
                _lastScrollTick = now;
                _scrollPage = (_scrollPage + 1) % 2;
                _showRunning();
            }
        }

        vTaskDelayUntil(&xLastWake, loopTick);
    }
}

/* _handleEjectButton() -- independant de la machine a etats principale */
/*
   Court : demande d'ejection securisee -> _safeEject = true, LED bleue fixe ON
   Long  : reset eject (carte reinseree)   -> _safeEject = false, LED bleue OFF
*/
void ButtonManager::_handleEjectButton(BtnEvent evt) {
    if (evt == BtnEvent::SHORT_PRESS) {
        WifiManager::setSafeEject(true);
        WifiManager::setRecordingSD(false);
        digitalWrite(LED_SAFE_EJECT, HIGH);
        Serial.println("[BTN] EJECT court -> safe eject ON");
    }
    else if (evt == BtnEvent::LONG_PRESS) {
        WifiManager::setSafeEject(false);
        digitalWrite(LED_SAFE_EJECT, LOW);
        Serial.println("[BTN] EJECT long -> safe eject OFF (carte reactivee)");
    }
}

/*_read() -- debounce generique SHORT / LONG */
BtnEvent ButtonManager::_read(BtnDebounce& db, uint8_t pin) {
    int        rawNow = digitalRead(pin);
    TickType_t now    = xTaskGetTickCount();

    if (rawNow != db.lastRaw) {
        db.lastRaw       = rawNow;
        db.debounceStart = now;
        return BtnEvent::NONE;
    }
    if ((now - db.debounceStart) < pdMS_TO_TICKS(BTN_DEBOUNCE_MS))
        return BtnEvent::NONE;

    /* Front descendant : bouton presse */
    if (rawNow == LOW && db.stableLevel == HIGH) {
        db.stableLevel  = LOW;
        db.pressStart   = now;
        db.pressHandled = false;
        return BtnEvent::NONE;
    }

    /* Front montant : bouton relache */
    if (rawNow == HIGH && db.stableLevel == LOW) {
        db.stableLevel = HIGH;
        if (!db.pressHandled) {
            db.pressHandled = true;
            uint32_t dur = pdTICKS_TO_MS(now - db.pressStart);
            Serial.printf("[BTN] pin %d relache %u ms\n", pin, dur);
            return (dur >= BTN_LONG_PRESS_MS) ? BtnEvent::LONG_PRESS
                                              : BtnEvent::SHORT_PRESS;
        }
    }
    return BtnEvent::NONE;
}

/*Helpers*/
int ButtonManager::_activeCount() const {
    int n = 0;
    for (int i = 0; i < 4; i++) if (_active[i]) n++;
    return n;
}

uint8_t ButtonManager::_buildMask() const {
    uint8_t m = 0;
    for (int i = 0; i < 4; i++) if (_active[i]) m |= (1 << i);
    return m;
}

void ButtonManager::_startRunning() {
    uint8_t mask = _buildMask();
    WifiManager::setSensorMask(mask);
    WifiManager::setRecordingSD(true);
    _scrollPage     = 0;
    _lastScrollTick = xTaskGetTickCount();
    _lastLcdTick    = xTaskGetTickCount();
    _state          = BtnState::RUNNING;
    Serial.printf("[BTN] RUNNING mask=0x%02X\n", mask);
    _showRunning();
}

void ButtonManager::_stopAcq() {
    WifiManager::setRecordingSD(false);
    _state = BtnState::WELCOME;
    WifiManager::lcdPrint(0, "  Acquisition   ");
    WifiManager::lcdPrint(1, "   arretee      ");
    vTaskDelay(pdMS_TO_TICKS(2000));
    _showWelcome();
    Serial.println("[BTN] acquisition arretee -> WELCOME");
}

/*Affichages LCD */
void ButtonManager::_showWelcome() {
    char l0[17], l1[17];
    snprintf(l0, sizeof(l0), "> J%d  [%s]",
             _cursor + 1, _active[_cursor] ? "ON " : "OFF");
    snprintf(l1, sizeof(l1), "SCR=+/long=ON/OFF");
    WifiManager::lcdPrint(0, l0);
    WifiManager::lcdPrint(1, l1);
}

void ButtonManager::_showRunning() {
    uint8_t mask = WifiManager::getSensorMask();
    char l0[17], l1[17];
    float v[4];
    for (int i = 0; i < 4; i++) v[i] = WifiManager::getGaugeValue(i);

    int active[4], nActive = 0;
    for (int i = 0; i < 4; i++)
        if (mask & (1 << i)) active[nActive++] = i;

    if (nActive == 0) {
        WifiManager::lcdPrint(0, "  Aucun capteur ");
        WifiManager::lcdPrint(1, "                ");
        return;
    }
    if (nActive == 1) {
        snprintf(l0, sizeof(l0), "J%d:%+9.5fV", active[0] + 1, v[active[0]]);
        snprintf(l1, sizeof(l1), "MES=stop/susp.  ");
    }
    else if (nActive == 2) {
        snprintf(l0, sizeof(l0), "J%d:%+9.5fV", active[0] + 1, v[active[0]]);
        snprintf(l1, sizeof(l1), "J%d:%+9.5fV", active[1] + 1, v[active[1]]);
    }
    else {
        int base = _scrollPage * 2;
        int i0 = (base     < nActive) ? active[base]     : -1;
        int i1 = (base + 1 < nActive) ? active[base + 1] : -1;
        if (i0 >= 0) snprintf(l0, sizeof(l0), "J%d:%+9.5fV", i0+1, v[i0]);
        else         snprintf(l0, sizeof(l0), "                ");
        if (i1 >= 0) snprintf(l1, sizeof(l1), "J%d:%+9.5fV", i1+1, v[i1]);
        else         snprintf(l1, sizeof(l1), "MES=stop/susp.  ");
    }
    WifiManager::lcdPrint(0, l0);
    WifiManager::lcdPrint(1, l1);
}

void ButtonManager::_showSuspended() {
    WifiManager::lcdPrint(0, "  *** PAUSE *** ");
    WifiManager::lcdPrint(1, "MES court/long  ");
}