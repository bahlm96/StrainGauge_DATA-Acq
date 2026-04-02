/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim                           ************************************************************/
/********************************************* Description                      : Configuration ADS1256           ************************************************************/
/********************************************* Date de création(fr)             : 19/02/2026                      ************************************************************/
/********************************************* Date de denièrec modification    : 25/03/2026                      ************************************************************/

/* Changelog */                                                                                     /* Commentaire Ajouter */
/*                                                       
        Version  0.0.0   ----------------------------------------------------------------------->   : Version initiale 
        Version  0.0.1   ----------------------------------------------------------------------->   : L'Ajoute de la deuxième Jauge de contrainte 
        Version  0.0.2   ----------------------------------------------------------------------->   : L'Ajoute de l'equation de calcule de la déformation en unité (1 microstrain = 10^-6)
        Version  0.1.0   ----------------------------------------------------------------------->   : Creation d'un nouveau fichier headers (BUSspi) pour le bus SPI & mise en forme
        Version  0.1.1   ----------------------------------------------------------------------->   : L'ajout des defines de datasheet ADS1256
        */

/* A noter que les données de configuration de l'ADS1256 sont issus du datasheet -->  copyright © 2025, Texas Instruments Incorporated :: version Last updated 10/2025      */
/* Lien : https://www.ti.com/lit/ds/symlink/ads1256.pdf*/


/***************************************************************Résumé du code version FR************************************************************************************/
/*Ce projet implante un driver Arduino pour le convertisseur analogique-numérique (ADC) ADS1256, un composant de haute précision (24 bits). 
Le code permet de configurer le gain, de lire les registres internes et d'extraire des mesures de tension réelles.Points Clés du Code :Communication SPI : 
Le microcontrôleur communique avec l'ADS1256 via le bus SPI (Mode 1), en utilisant des broches spécifiques (CS sur 5, DRDY sur 4).Gestion du Gain (PGA) : 
Le code inclut une fonction permettant de convertir les bits de configuration bruts en valeurs de gain réelles (de 1 à 64). 
Ce gain est essentiel pour diviser la valeur brute et obtenir la tension correcte.Lecture 24 bits : 
La fonction readADC() attend que le signal DRDY (Data Ready) passe à l'état bas, puis récupère 3 octets de données. 
Elle effectue ensuite une extension de signe pour transformer ces 24 bits en un entier 32 bits signé, permettant de gérer les tensions négatives.Conversion en Tension :
 Dans la boucle principale, le code calcule la tension finale en utilisant le Quantum (basé sur une référence de 2.5V) et le gain configuré :
                                                       Tension = (ValeurADC * Quantum)/ ( GAIN)
 Affichage : Les données sont envoyées au moniteur série avec un formatage compatible avec l'outil Teleplot (>Voltage:valeur) pour une visualisation graphique en temps réel.*/
 /****************************************************************************************************************************************************************************/
/****************************************************************************************************************************************************************************/


/****************************************************************Résumé du code version ENG********************************************************************************/
/*This project implements an Arduino driver for the ADS1256, a high-precision 24-bit Analog-to-Digital Converter (ADC). The code handles gain configuration, internal register access, 
and the extraction of real-world voltage measurements.Key Features:SPI Communication: The microcontroller communicates with the ADS1256 
using the SPI bus (Mode 1) via dedicated pins (CS on 5, DRDY on 4).Programmable Gain (PGA) Management: 
A specific function converts raw configuration bits into actual gain values (ranging from 1 to 64). 
This gain is used to scale the raw ADC data back to the correct voltage level.24-bit Data Acquisition: 
The readADC() function waits for the DRDY (Data Ready) pin to go LOW, then pulls 3 bytes of data. 
It performs sign extension to convert the 24-bit result into a signed 32-bit integer, ensuring support for differential/negative signals.Voltage Calculation: 
In the main loop, the software calculates the actual voltage using the Quantum (defined by a 2.5V reference) and the active gain:
                                                        Tension = (ValeurADC * Quantum)/ ( GAIN)
Data Visualization: Results are printed to the Serial monitor using a format compatible with Teleplot (>Voltage:value), allowing for real-time graphical plotting.*/
/****************************************************************************************************************************************************************************/

#ifndef ADS1256_H                                              // Évite l’inclusion multiple du fichier header
#define ADS1256_H                                              // Définit le macro pour protection
#include <Arduino.h>                                           // Bibliothèque de base Arduino
#include <SPI.h>                                               // Bibliothèque SPI (communication avec ADS1256)

extern SPIClass spiADS;
// COMMANDES ADS1256----> voir le datasheet pour bien configurer ---- voir le lien ci-dessus 

/**************************************************************Gain**********************************************************/
#define GAIN_1 0x00                                             // Le gain vaut 1
#define GAIN_2 0x01                                             // Le gain vaut 2
#define GAIN_4 0x02                                             // Le gain vaut 4
#define GAIN_8 0x03                                             // Le gain vaut 8
#define GAIN_16 0x04                                            // Le gain vaut 16
#define GAIN_32 0x05                                            // Le gain vaut 32
#define GAIN_64 0x06                                            // Le gain vaut 64



/***************************************************Fréquence d'échantillonage***********************************************************************/
#define ADS1256_DRATE_30000   0xF0
#define ADS1256_DRATE_15000   0xE0
#define ADS1256_DRATE_7500    0xD0
#define ADS1256_DRATE_3750    0xC0
#define ADS1256_DRATE_2000    0xB0
#define ADS1256_DRATE_1000    0xA1
#define ADS1256_DRATE_500     0x92
#define ADS1256_DRATE_100     0x82
#define ADS1256_DRATE_60      0x72
#define ADS1256_DRATE_50      0x63
#define ADS1256_DRATE_30      0x53
#define ADS1256_DRATE_25      0x43
#define ADS1256_DRATE_15      0x33
#define ADS1256_DRATE_10      0x23
#define ADS1256_DRATE_5       0x13
#define ADS1256_DRATE_2_5     0x03


/*********************************************************** MUX CANAL *******************************************************************************/
#define ADS1256_MUX_AIN0     0x00
#define ADS1256_MUX_AIN1     0x01
#define ADS1256_MUX_AIN2     0x02
#define ADS1256_MUX_AIN3     0x03
#define ADS1256_MUX_AIN4     0x04
#define ADS1256_MUX_AIN5     0x05
#define ADS1256_MUX_AIN6     0x06
#define ADS1256_MUX_AIN7     0x07
#define ADS1256_MUX_AINCOM   0x08


/****************************************************************Command*******************************************************************************/
#define CMD_RREG    0x10                                        // Lire registre interne
#define CMD_WREG    0x50                                        // Écrire registre
#define CMD_RDATA   0x01                                        // Lire une seule conversion
#define CMD_SYNC    0xFC                                        // Voir dans command definitions sur Datasheet page 34
#define CMD_WAKEUP  0x00                                        // Pareil page 34


/*****************************************************************Registre******************************************************************************/
#define REG_MUX     0x01                                        // Sélection des entrées AINP et AINN
#define REG_ADCON   0x02                                        // PGA + Clock + Sensor detect
#define ADCON_RESET 0x20
#define REG_DRATE   0x03                                        // Data rate (SPS) ----configuration du débit d’échantillonnage -------> 2.5 échantillons/s 
#define REG_STATUS  0x00
#define REG_To_Write 0x00


/************************************************************Chip Select & DATA READY********************************************************************/
#define CS_PIN 5                                               // Broche Chip Select_ADS1256 (active LOW)
#define DRDY_PIN 4                                              // Broche Data Ready (LOW = conversion prête)

/********************************************************************IO REGISTER*************************************************************************/
#define ADS1256_IO_DIR_MASK   0xF0
#define ADS1256_IO_VALUE_MASK 0x0F

/********************************************************* configurer MUX (AINP, AINN)*********************************************************/
#define ADS1256_MUX_DIFF(pos, neg)   (((pos) << 4) | (neg))



/*****************************************************************Masques***********************************************************************/

#define Masque_BITS  0xFF
#define Sign_Of_24_Bit  0x800000
#define Extension_Sign_Bit 0xFF000000



/***************************************************************ADCON REGISTER BITS*******************************************************************/
#define ADCON_CLK_OFF 0b0000000                                 // Horloge interne désactivée (rarement utilisé)
#define ADCON_CLK_DFLT 0b0010000                                // Horloge interne par défaut
#define ADCON_CLK_DFLT_DEVIDED_BY_2 0b0100000                   // Horloge divisée par 2
#define ADCON_CLK_DFLT_DEVIDED_BY_4 0b0110000                   // Horloge divisée par 4



/********************************************************************Sensor Detect Current Sources***************************************************/
#define ADCON_SDCS_OFF 0b00000000                               // Source de courant désactivée
#define ADCON_SDCS_0_5_MIC_A 0b00001000                         // 0.5 µA injecté
#define ADCON_SDCS_2_MIC_A 0b00010000                           // 2 µA injecté
#define ADCON_SDCS_10_MIC_A 0b00011000                          // 10 µA injecté


#define MAX_VALUE_23_BIT_1 0x7FFFFF                              // Valeur maximale d’un ADC 24 bits signé (2^23 - 1 = 8 388 607)            

/****************************************************************Read 32 Octets On ADS1256 *************************************************************************/

#define Read_First_Octets 16 
#define Read_Second_Octets 8 














#endif                                                          // Fin protection contre inclusion multiple