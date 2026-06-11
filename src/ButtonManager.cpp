/**************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                       ********************************************/
/********************************************* Fichier                          : ButtonManager.cpp                 ********************************************/
/********************************************* Date de création                 : 11/06/2026                       *********************************************/

#include "ButtonManager.hpp"
using State = ButtonManager::State;
#include "WiFiManager.hpp"

/* ── Index boutons dans les tableaux ──────────────────────────────────────── */
#define IDX_PREV    0
#define IDX_SUSPEND 1
#define IDX_STOP    2
#define IDX_NEXT    3

static const uint8_t BTN_PINS[4] = { BTN_PREV, BTN_SUSPEND, BTN_STOP, BTN_NEXT };

/* ═══════════════════════════════════════════════════════════════════════════ */

ButtonManager::ButtonManager()
    : _state(State::MENU), _cursor(0), _nextPressStart(0), _nextHeld(false)
{
    for (int i = 0; i < 4; i++) {
        _active[i]    = true;    // tous les capteurs actifs par défaut
        _lastPress[i] = 0;
        _pressed[i]   = false;
    }
}

/* ── begin() ─────────────────────────────────────────────────────────────── */
void ButtonManager::begin() {
    for (uint8_t pin : BTN_PINS) {
        // GPIO 34 et 35 sont input-only sur ESP32 → pas de pull-up interne possible
        if (pin == 34 || pin == 35) {
            pinMode(pin, INPUT);   // pull-up externe obligatoire sur ces pins
        } else {
            pinMode(pin, INPUT_PULLUP);
        }
    }

    xTaskCreatePinnedToCore(
        ButtonManager::taskEntry,
        "BtnTask",
        2048,
        this,
        2,          // priorité 2 — au-dessus du blink LED, en-dessous de l'acq.
        nullptr,
        1           // Cœur 1
    );
}

/* ── Entrée de la tâche ───────────────────────────────────────────────────── */
void ButtonManager::taskEntry(void* pv) {
    ((ButtonManager*)pv)->run();
}

/* ── Boucle principale ────────────────────────────────────────────────────── */
void ButtonManager::run() {
    _drawMenu();   // affichage initial

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));   // période de polling 20 ms

        /* ── Lecture et dispatch des boutons ─────────────────────────── */
        if (_read(BTN_PREV,    IDX_PREV))    _onPrev();
        if (_read(BTN_SUSPEND, IDX_SUSPEND)) _onSuspend();
        if (_read(BTN_STOP,    IDX_STOP))    _onStop();

        /* NEXT : gestion appui court ET appui long */
        bool nextRaw = (digitalRead(BTN_NEXT) == LOW);

        if (nextRaw && !_nextHeld) {
            // Début d'appui
            _nextHeld        = true;
            _nextPressStart  = millis();
        } else if (!nextRaw && _nextHeld) {
            // Relâché
            uint32_t held = millis() - _nextPressStart;
            _nextHeld = false;
            if (held >= LONG_PRESS_MS) {
                _onNextLong();
            } else if (held >= DEBOUNCE_MS) {
                _onNext();
            }
        }
    }
}

/* ── Lecture filtrée d'un bouton (front descendant) ─────────────────────── */
/*
  Retourne true UNE SEULE FOIS par appui (front descendant filtré).
  Niveau bas = appui (INPUT_PULLUP ou pull-up externe).
*/
bool ButtonManager::_read(uint8_t pin, uint8_t idx) {
    bool raw = (digitalRead(pin) == LOW);

    if (raw && !_pressed[idx]) {
        uint32_t now = millis();
        if (now - _lastPress[idx] > DEBOUNCE_MS) {
            _pressed[idx]   = true;
            _lastPress[idx] = now;
            return true;   // front détecté
        }
    } else if (!raw) {
        _pressed[idx] = false;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
   Handlers boutons
   ═══════════════════════════════════════════════════════════════════════════ */

/* ── PREV ────────────────────────────────────────────────────────────────── */
void ButtonManager::_onPrev() {
    switch (_state) {
        case State::MENU:
            // Pas d'écran précédent — rien à faire (ou déplacer curseur en arrière)
            if (_cursor > 0) { _cursor--; _drawMenu(); }
            break;
        case State::RUNNING:
        case State::PAUSED:
            _stopAcq();
            break;
    }
}

/* ── SUSPEND ─────────────────────────────────────────────────────────────── */
void ButtonManager::_onSuspend() {
    switch (_state) {
        case State::MENU:
            /* Toggle ON/OFF du capteur sous le curseur */
            _active[_cursor] = !_active[_cursor];
            _drawMenu();
            break;

        case State::RUNNING:
            /* Pause : stoppe l'écriture SD sans réinitialiser */
            WifiManager::setRecordingSD(false);
            _state = State::PAUSED;
            _drawPaused();
            Serial.println("[BTN] Pause SD");
            break;

        case State::PAUSED:
            /* Reprise */
            WifiManager::setRecordingSD(true);
            _state = State::RUNNING;
            _drawRunning();
            Serial.println("[BTN] Reprise SD");
            break;
    }
}

/* ── STOP ─────────────────────────────────────────────────────────────────── */
void ButtonManager::_onStop() {
    if (_state == State::RUNNING || _state == State::PAUSED) {
        _stopAcq();
    }
    // En State::MENU : ignoré
}

/* ── NEXT (appui court) ───────────────────────────────────────────────────── */
void ButtonManager::_onNext() {
    if (_state == State::MENU) {
        _cursor = (_cursor + 1) % 4;   // J1 → J2 → J3 → J4 → J1
        _drawMenu();
    }
    // En RUNNING / PAUSED : ignoré
}

/* ── NEXT (appui long ≥ 800 ms) → lancer acquisition ─────────────────────── */
void ButtonManager::_onNextLong() {
    if (_state == State::MENU) {
        /* Vérifie qu'au moins un capteur est actif */
        if (_buildMask() == 0) {
            /* Aucun capteur sélectionné — clignotement LCD d'avertissement */
            WifiManager::lcdPrint(1, "  Aucun capteur!");
            vTaskDelay(pdMS_TO_TICKS(1200));
            _drawMenu();
            return;
        }
        _startAcq();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   Actions SD
   ═══════════════════════════════════════════════════════════════════════════ */

void ButtonManager::_startAcq() {
    uint8_t mask = _buildMask();
    WifiManager::setSensorMask(mask);
    WifiManager::setRecordingSD(true);
    _state = State::RUNNING;
    _drawRunning();
    Serial.printf("[BTN] Acquisition lancee — mask=0x%02X\n", mask);
}

void ButtonManager::_stopAcq() {
    WifiManager::setRecordingSD(false);
    _state = State::MENU;
    _drawMenu();
    Serial.println("[BTN] Acquisition arretee");
}

/* ── Construit le masque uint8_t depuis _active[] ────────────────────────── */
uint8_t ButtonManager::_buildMask() const {
    uint8_t m = 0;
    for (int i = 0; i < 4; i++) {
        if (_active[i]) m |= (1 << i);
    }
    return m;
}

/* ═══════════════════════════════════════════════════════════════════════════
   Affichage LCD
   ═══════════════════════════════════════════════════════════════════════════ */

/*
  Ligne 0 : "J1[x] J2[x] J3   "   (x = actif, espace = inactif)
  Ligne 1 : "J4    >Lancer<   "    (curseur sur J4)

  Disposition 4 capteurs sur 2 lignes (2 par ligne) :
    Ligne 0 → J1 J2
    Ligne 1 → J3 J4

  Chaque cellule occupe 8 caractères : "J1[x]   " ou ">J1[x]< "
*/
void ButtonManager::_drawMenu() {
    /* Formate une cellule capteur : ">J1[x]< " si curseur, "J1[x]  " sinon */
    auto cell = [&](uint8_t idx) -> String {
        bool cur = (_cursor == idx);
        bool act = _active[idx];
        String s = "";
        s += cur ? ">" : " ";
        s += "J";
        s += String(idx + 1);
        s += act ? "[x]" : "[ ]";
        s += cur ? "<" : " ";
        return s;   // 7 chars
    };

    // Ligne 0 : J1 J2
    String l0 = cell(0) + " " + cell(1);
    // Ligne 1 : J3 J4
    String l1 = cell(2) + " " + cell(3);

    WifiManager::lcdPrint(0, l0.c_str());
    WifiManager::lcdPrint(1, l1.c_str());
}

void ButtonManager::_drawRunning() {
    /* Ligne 0 : "Acq. en cours   " */
    WifiManager::lcdPrint(0, "Acq. en cours");

    /* Ligne 1 : liste des capteurs actifs ex "J1 J2 J4        " */
    String l1 = "";
    for (int i = 0; i < 4; i++) {
        if (_active[i]) { l1 += "J"; l1 += String(i + 1); l1 += " "; }
    }
    WifiManager::lcdPrint(1, l1.c_str());
}

void ButtonManager::_drawPaused() {
    WifiManager::lcdPrint(0, "Pause SD");
    WifiManager::lcdPrint(1, "SUSP=reprendre");
}