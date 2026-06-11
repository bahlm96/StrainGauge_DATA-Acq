/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : MemoryDevice.cpp                 ************************************************************/
/********************************************* Date de dernière modification    : 05/06/2026                      ************************************************************/


#include <MemoryDevice.hpp>
#include "WiFiManager.hpp"

CardSD::CardSD(SPIClass* spi, uint8_t cs, String filename)
    : _spi(spi), _csPin(cs), _filename(filename), _mask(0x0F) {}

/* _writeHeader() — En-tête CSV dynamique selon _mask*/
void CardSD::_writeHeader() {
    File f = SD.open(_filename, FILE_WRITE);
    if (!f) {
        Serial.printf("[SD] Erreur ouverture pour en-tête : %s\n", _filename.c_str());
        return;
    }
    f.print("TimeStamp");
    for (int i = 0; i < 4; i++) {
        if (_mask & (1 << i)) {
            f.printf(",Jauge%d", i + 1);
        }
    }
    f.println();
    f.close();
    Serial.printf("[SD] En-tête écrit dans %s (mask=0x%02X)\n", _filename.c_str(), _mask);
}

/*  begin()  */
bool CardSD::begin(uint8_t mask) {
    _mask = mask & 0x0F;

    if (!SD.begin(_csPin, *_spi)) {
        Serial.println("[SD] Échec SD.begin()");
        return false;
    }

    /* Créer le dossier  si absent */
    int lastSlash = _filename.lastIndexOf('/');
    if (lastSlash > 0) {
        String folder = _filename.substring(0, lastSlash);
        if (!SD.exists(folder)) {
            if (SD.mkdir(folder)) {
                Serial.printf("[SD] Dossier créé : %s\n", folder.c_str());
            } else {
                Serial.printf("[SD] Impossible de créer : %s\n", folder.c_str());
            }
        }
    }

    /* En-tête si fichier absent ou vide */
    bool needHeader = false;
    if (!SD.exists(_filename)) {
        needHeader = true;
    } else {
        File tmp = SD.open(_filename, FILE_READ);
        if (tmp) {
            needHeader = (tmp.size() == 0);
            tmp.close();
        }
    }

    if (needHeader) {
        _writeHeader();
    }

    Serial.printf("[SD] Carte OK — fichier : %s (mask=0x%02X)\n", _filename.c_str(), _mask);
    return true;
}

/* ── setMask() — Change le masque en cours de session ───────────────────── */
/*
  Si le masque change (nouvelle sélection de capteurs depuis l'IHM) :
     le fichier actuel est fermé (les données déjà écrites sont conservées)
     un nouveau fichier est créé avec l'en-tête correspondant au nouveau masque

  Appelé depuis MemoryManager::run() avant saveBuffer() quand WifiManager::getSensorMask()
  a changé depuis le dernier flush.
*/
void CardSD::setMask(uint8_t mask) {
    mask = mask & 0x0F;
    if (mask == _mask) return;   // pas de changement, rien à faire

    Serial.printf("[SD] Masque changé 0x%02X → 0x%02X : nouveau fichier\n", _mask, mask);
    _mask = mask;

    /* Génère un nouveau nom de fichier pour ne pas écraser les données précédentes */
    /* Stratégie simple : ajoute un suffixe horodaté basé sur millis() */
    int lastDot = _filename.lastIndexOf('.');
    String base = (lastDot > 0) ? _filename.substring(0, lastDot) : _filename;
    String ext  = (lastDot > 0) ? _filename.substring(lastDot)    : ".csv";
    _filename = base + "_" + String(millis() / 1000) + "s" + ext;

    _writeHeader();
}

/* saveRow()*/
void CardSD::saveRow(String dataCSVRow) {
    File f = SD.open(_filename, FILE_APPEND);
    if (f) {
        f.println(dataCSVRow);
        f.close();
    } else {
        Serial.println("[SD] Erreur ouverture (saveRow) : " + _filename);
    }
}

/* saveBuffer() — Structure dynamique selon _mask  */

void CardSD::saveBuffer(double* buffer, int taille) {
    if (!buffer) return;

    /* Détection d'un changement de masque depuis l'IHM */
    uint8_t currentMask = WifiManager::getSensorMask();
    if (currentMask != _mask) {
        setMask(currentMask);
    }

    int stride = 1 + __builtin_popcount(_mask);
    if (stride <= 1) return;   // mode visu seule, rien à écrire

    File f = SD.open(_filename, FILE_APPEND);
    if (!f) {
        Serial.println("[SD] Erreur ouverture (saveBuffer) : " + _filename);
        return;
    }

    for (int i = 0; i + stride <= taille; i += stride) {
        /* Timestamp */
        f.printf("%.0f", buffer[i]);

        /* Valeurs capteurs actifs dans l'ordre J1→J4 */
        int idx = 1;
        for (int ch = 0; ch < 4; ch++) {
            if (_mask & (1 << ch)) {
                f.printf(",%.6f", buffer[i + idx]);
                idx++;
            }
        }
        f.println();
    }

    f.close();
}
