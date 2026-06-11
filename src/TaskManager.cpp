/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : TaskManager.cpp                 ************************************************************/
/********************************************* Date de dernière modification    : 05/06/2026                      ************************************************************/


#include "TaskManager.hpp"
#include "MemoryManager.hpp"
#include "InputReaderManager.hpp"
#include "WiFiManager.hpp"
#include <SD.h>

/* Constructeur  */
TaskManager::TaskManager(InputReaderManager& inputReaderManager, MemoryManager& memoryManager)
    : _inputReaderManager(&inputReaderManager),
      _memoryManager(&memoryManager)
{}

/*begin() Crée la tâche FreeRTOS  */
void TaskManager::begin() {
    xTaskCreatePinnedToCore(taskWrapper, "StorageTask", 8192, this, 1, nullptr, 1);
}

/*taskWrapper() — Adaptateur C pour FreeRTOS */
void TaskManager::taskWrapper(void* pvParameters) {
    TaskManager* self = static_cast<TaskManager*>(pvParameters);
    self->run();
}

/*writeCSVHeader() — En-tête CSV selon le masque  */

static void writeCSVHeader(File& f, uint8_t mask) {
    f.print("timestamp_ms");
    for (int i = 0; i < 4; i++) {
        if (mask & (1 << i)) {
            f.printf(",Jauge_%d", i + 1);
        }
    }
    f.println();
}

/* ── writeCSVLine() — Une ligne de données depuis le buffer ──────────────── */

static int writeCSVLine(File& f, const double* ptr, uint8_t mask) {
    int idx = 0;

    // Timestamp
    f.printf("%.2f", ptr[idx++]);

    // Valeurs capteurs actifs dans l'ordre J1→J4
    for (int i = 0; i < 4; i++) {
        if (mask & (1 << i)) {
            f.printf(",%.6f", ptr[idx++]);
        }
    }
    f.println();

    return idx;
}

/* ── run() — Boucle principale de la tâche SD ───────────────────────────── */
/*La tâche dort jusqu'à ce que InputReaderManager::_switchBuffer() l'éveille via xTaskNotifyGive(). Elle consomme alors le buffer plein et écrit le CSV.*/
void TaskManager::run() {

    /* Enregistre le handle de cette tâche dans InputReaderManager
       pour que _switchBuffer() puisse la réveiller directement.     */
    _inputReaderManager->setStorageTaskHandle(xTaskGetCurrentTaskHandle());

    File    csvFile;
    uint8_t fileMask  = 0xFF;   // masque utilisé pour l'en-tête du fichier ouvert (0xFF = pas de fichier)
    int     fileIndex = 0;      // compteur pour nommer les fichiers acq_001.csv, acq_002.csv...

    while (true) {

        /* Attente bloquante — réveil par xTaskNotifyGive() dans _switchBuffer() */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Snapshot du masque et du buffer au moment du réveil */
        uint8_t mask   = _inputReaderManager->getActiveMask();
        double* buf    = _inputReaderManager->getBufferReady();
        int     size   = _inputReaderManager->getBufferSize();

        /* stride = nombre de doubles par cycle dans le buffer
           Si mode "visu seule" → stride = 1 (timestamp seul, ou 0 si pas de timestamp)
           → rien à écrire sur SD, on ignore ce buffer.                               */
        int stride = 1 + __builtin_popcount(mask);
        if (!WifiManager::isRecordingSD() || stride <= 1 || buf == nullptr) {
            if (csvFile) { csvFile.close(); fileMask = 0xFF; }
            continue;
        }

        /* Si le masque a changé depuis l'ouverture du fichier → nouveau fichier */
        if (csvFile && mask != fileMask) {
            csvFile.close();
            fileMask = 0xFF;
            Serial.printf("[TaskManager] Masque changé (0x%02X→0x%02X) : nouveau fichier.\n",
                          fileMask, mask);
        }

        /* Ouverture du fichier CSV si nécessaire */
        if (!csvFile) {
            char path[48];
            fileIndex++;
            snprintf(path, sizeof(path), "/Acquisition_Folder/acq_%03d.csv", fileIndex);
            csvFile = SD.open(path, FILE_WRITE);
            if (!csvFile) {
                Serial.printf("[TaskManager] Erreur ouverture %s\n", path);
                continue;
            }
            fileMask = mask;
            writeCSVHeader(csvFile, mask);
            Serial.printf("[TaskManager] Nouveau fichier : %s (mask=0x%02X, stride=%d)\n",
                          path, mask, stride);
        }

        /* Vidage du buffer ligne par ligne */
        int i = 0;
        while (i + stride <= size) {
            i += writeCSVLine(csvFile, buf + i, mask);
        }

        csvFile.flush();
    }
}
