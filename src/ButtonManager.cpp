/******************************************************************************/
/* Auteur  : Halim BALA                                                       */
/* Fichier : ButtonManager.cpp                                                */
/* Date    : 12/06/2026                                                       */
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
      _confirmStart(0),
      _dbNext (dbInit(BTN_NEXT)),
      _dbValid(dbInit(BTN_VALID)),
      _dbPrev (dbInit(BTN_PREV)),
      _dbStop (dbInit(BTN_STOP))
{}

/* begin()*/
void ButtonManager::begin() {
    /* GPIO 15, 21, 22; 34 */
    pinMode(BTN_NEXT,  INPUT_PULLUP);
    pinMode(BTN_VALID, INPUT_PULLUP);
    pinMode(BTN_PREV,  INPUT_PULLUP);
    pinMode(BTN_STOP,  INPUT);

    /* Relecture apres pinMode */
    _dbNext  = dbInit(BTN_NEXT);
    _dbValid = dbInit(BTN_VALID);
    _dbPrev  = dbInit(BTN_PREV);
    _dbStop  = dbInit(BTN_STOP);

    _showWelcome();

    xTaskCreatePinnedToCore(_taskWrapper, "BtnTask", 2048, this, 1, nullptr, 1);
    Serial.println("[BTN] demarrage : NEXT=15 VALID=21 PREV=22 STOP=34");
}

void ButtonManager::_taskWrapper(void* pv) {
    static_cast<ButtonManager*>(pv)->_run();
}

/*  _run() */
void ButtonManager::_run() {
    const TickType_t loopTick = pdMS_TO_TICKS(10);
    TickType_t xLastWake = xTaskGetTickCount();

    while (true) {

        BtnEvent evtNext  = _read(_dbNext,  BTN_NEXT);
        BtnEvent evtValid = _read(_dbValid, BTN_VALID);
        BtnEvent evtPrev  = _read(_dbPrev,  BTN_PREV);
        BtnEvent evtStop  = _read(_dbStop,  BTN_STOP);

        switch (_state) {

            /* WELCOME*/
            /* N'importe quel bouton -> SELECT                         */
            case BtnState::WELCOME:
                if (evtNext  != BtnEvent::NONE ||
                    evtValid != BtnEvent::NONE ||
                    evtPrev  != BtnEvent::NONE ||
                    evtStop  != BtnEvent::NONE) {
                    _cursor = 0;
                    _state  = BtnState::SELECT;
                    Serial.println("[BTN] WELCOME -> SELECT");
                    _showSelect();
                }
                break;

            /* -- SELECT --------------------------------------------- */
            /*
               NEXT  court : curseur suivant J1->J2->J3->J4->J1
               VALID court : ON / OFF la jauge affichee
               VALID long  : valide la selection et lance la mesure
               STOP        : retour WELCOME
            */
            case BtnState::SELECT:
                if (evtNext == BtnEvent::SHORT_PRESS ||
                    evtNext == BtnEvent::LONG_PRESS) {
                    _cursor = (_cursor + 1) % 4;
                    Serial.printf("[BTN] SELECT cursor J%d\n", _cursor + 1);
                    _showSelect();
                }
                else if (evtValid == BtnEvent::SHORT_PRESS) {
                    _active[_cursor] = !_active[_cursor];
                    Serial.printf("[BTN] SELECT J%d %s\n",
                                  _cursor + 1, _active[_cursor] ? "ON" : "OFF");
                    _showSelect();
                }
                else if (evtValid == BtnEvent::LONG_PRESS) {
                    if (_activeCount() == 0) _active[0] = true;
                    Serial.println("[BTN] SELECT -> RUNNING (VALID long)");
                    _startRunning();
                }
                else if (evtStop != BtnEvent::NONE) {
                    _state = BtnState::WELCOME;
                    Serial.println("[BTN] SELECT -> WELCOME");
                    _showWelcome();
                }
                break;

            /* RUNNING */
            /*
               PREV  court : pause
               PREV  long  : (deja en cours, ignoré)
               STOP        : arret definitif -> WELCOME
               NEXT        : demande confirmation quitter
            */
            case BtnState::RUNNING:
                if (evtPrev == BtnEvent::SHORT_PRESS) {
                    WifiManager::setRecordingSD(false);
                    _state = BtnState::SUSPENDED;
                    Serial.println("[BTN] RUNNING -> SUSPENDED");
                    _showSuspended();
                }
                else if (evtStop != BtnEvent::NONE) {
                    _stopAcq();
                }
                else if (evtNext != BtnEvent::NONE) {
                    _confirmStart = xTaskGetTickCount();
                    _state = BtnState::CONFIRM;
                    Serial.println("[BTN] RUNNING -> CONFIRM");
                    _showConfirm();
                }
                break;

            /* SUSPENDED */
            /*
               PREV  court : reprend la mesure
               PREV  long  : relance la mesure (restart)
               STOP        : arret definitif -> WELCOME
               NEXT        : demande confirmation quitter
            */
            case BtnState::SUSPENDED:
                if (evtPrev == BtnEvent::SHORT_PRESS) {
                    WifiManager::setRecordingSD(true);
                    _state = BtnState::RUNNING;
                    _lastLcdTick    = xTaskGetTickCount();
                    _lastScrollTick = xTaskGetTickCount();
                    Serial.println("[BTN] SUSPENDED -> RUNNING");
                    _showRunning();
                }
                else if (evtPrev == BtnEvent::LONG_PRESS) {
                    Serial.println("[BTN] SUSPENDED -> RUNNING (relance)");
                    _startRunning();
                }
                else if (evtStop != BtnEvent::NONE) {
                    _stopAcq();
                }
                else if (evtNext != BtnEvent::NONE) {
                    _confirmStart = xTaskGetTickCount();
                    _state = BtnState::CONFIRM;
                    Serial.println("[BTN] SUSPENDED -> CONFIRM");
                    _showConfirm();
                }
                break;

            /* CONFIRM  */
            /*
               VALID long  : confirme -> arret + WELCOME
               PREV  court : annule   -> reprend RUNNING
               Timeout 3 s : annule automatiquement
            */
            case BtnState::CONFIRM:
                if (evtValid == BtnEvent::LONG_PRESS) {
                    Serial.println("[BTN] CONFIRM oui -> STOP");
                    _stopAcq();
                }
                else if (evtPrev != BtnEvent::NONE) {
                    WifiManager::setRecordingSD(true);
                    _state = BtnState::RUNNING;
                    _lastLcdTick    = xTaskGetTickCount();
                    _lastScrollTick = xTaskGetTickCount();
                    Serial.println("[BTN] CONFIRM annule -> RUNNING");
                    _showRunning();
                }
                else {
                    TickType_t elapsed = xTaskGetTickCount() - _confirmStart;
                    if (elapsed >= pdMS_TO_TICKS(CONFIRM_TIMEOUT_MS)) {
                        WifiManager::setRecordingSD(true);
                        _state = BtnState::RUNNING;
                        _lastLcdTick    = xTaskGetTickCount();
                        _lastScrollTick = xTaskGetTickCount();
                        Serial.println("[BTN] CONFIRM timeout -> RUNNING");
                        _showRunning();
                    }
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
    WifiManager::lcdPrint(0, "    Machine     ");
    WifiManager::lcdPrint(1, "    Control     ");
}

void ButtonManager::_showSelect() {
    char l0[17], l1[17];
    snprintf(l0, sizeof(l0), "> J%d  [%s]",
             _cursor + 1, _active[_cursor] ? "ON " : "OFF");
    snprintf(l1, sizeof(l1), "VAL=ON/OFF long=");
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
        snprintf(l1, sizeof(l1), "PRV=pause       ");
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
        else         snprintf(l1, sizeof(l1), "PRV=pause       ");
    }
    WifiManager::lcdPrint(0, l0);
    WifiManager::lcdPrint(1, l1);
}

void ButtonManager::_showSuspended() {
    WifiManager::lcdPrint(0, "  *** PAUSE *** ");
    WifiManager::lcdPrint(1, "PRV=reprise long");
}

void ButtonManager::_showConfirm() {
    WifiManager::lcdPrint(0, "Quitter ?       ");
    WifiManager::lcdPrint(1, "VALlong=oui     ");
}