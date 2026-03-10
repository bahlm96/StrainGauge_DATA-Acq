/*****************************************************************************************************************************************************************************/
/********************************************* Auteur                           : Halim BALA                      ************************************************************/
/********************************************* Fichier                          : InputDevice.hpp                 ************************************************************/
/********************************************* Description                      : Configuration ADS1256***********************************************************/
/********************************************* Date de création(fr)             : 03/03/2026                      ************************************************************/
/********************************************* Date de dernière modification    : 04/03/2026                      ************************************************************/

/* Ce fichier définit l'interface de la classe ADS1256 et regroupe l'ensemble des constantes de configuration 
   (Registres, Gains, Commandes) nécessaires au pilotage du convertisseur via le bus SPI.   */

/****************************************************************************************************************************************************************************/
#ifndef INPUTDEVICE_HPP                                                                          // dire au compilatuer si le ce fichier existe djéa passe à directement à endif            
#define INPUTDEVICE_HPP

#include <Arduino.h>
#include <SPI.h>

#define REG_MUX     0x01
#define GAIN_1      0x00                                                                          // Le gain vaut 1
#define GAIN_2      0x01                                                                          // Le gain vaut 2
#define GAIN_4      0x02                                                                          // Le gain vaut 4
#define GAIN_8      0x03                                                                          // Le gain vaut 8
#define GAIN_16     0x04                                                                          // Le gain vaut 16
#define GAIN_32     0x05                                                                          // Le gain vaut 32
#define GAIN_64     0x06                                                                          // Le gain vaut 64
#define CMD_RREG    0x10                                                                          // Lire registre interne
#define CMD_WREG    0x50                                                                          // Écrire registre
#define CMD_RDATA   0x01                                                                          // Lire une seule conversion
#define REG_MUX     0x01                                                                          // Sélection des entrées AINP et AINN
#define REG_ADCON   0x02                                                                          // PGA + Clock + Sensor detect
#define REG_DRATE   0x03                                                                          // Data rate (SPS) ----configuration du débit d’échantillonnage -------> 2.5 échantillons/s 
#define REG_STATUS  0x00
#define CMD_SYNC    0xFC                                                                          // Voir dans command definitions sur Datasheet page 34
#define CMD_WAKEUP  0x00                                                                          // Pareil page 34
#define AIN0           0
#define AIN1           1
#define AIN2           2
#define AIN3           3                                                                          // Broche Data Ready (LOW = conversion prête)
#define ADCON_CLK_OFF               0b0000000                                                     // Horloge interne désactivée (rarement utilisé)
#define ADCON_CLK_DFLT              0b0010000                                                     // Horloge interne par défaut
#define ADCON_CLK_DFLT_DEVIDED_BY_2 0b0100000                                                     // Horloge divisée par 2
#define ADCON_CLK_DFLT_DEVIDED_BY_4 0b0110000                                                     // Horloge divisée par 4

/**********************************************************Sensor Detect Current Sources*********************************************************************/
#define ADCON_SDCS_OFF             0b00000000                                                     // Source de courant désactivée
#define ADCON_SDCS_0_5_MIC_A       0b00001000                                                     // 0.5 µA injecté
#define ADCON_SDCS_2_MIC_A         0b00010000                                                     // 2 µA injecté
#define ADCON_SDCS_10_MIC_A        0b00011000                                                     // 10 µA injecté





const double MAX_VALUE_23_BIT_1 =  0x7FFFFF;                                                      // Valeur maximale d’un ADC 24 bits signé (2^23 - 1 = 8 388 607)
const double vref = 2.5;                                                                          // Tension de référence utilisée ( 2.5V externe ou interne)
const double Quantum = vref / (double) (MAX_VALUE_23_BIT_1) ;                                     // Quantum = Vref / (2^23 - 1) ---->  Permet de convertir la valeur brute ADC en tension réelle


/************************************************************Creation d'une class propre à l'ADS****************************************************************/
class ADS1256 {                                                                                   
private:
    uint8_t _csPin;
    uint8_t _drdyPin;
    SPIClass* _spi;
    uint32_t SPI_Speed;
    const double _vref;
    const double _quantum = _vref / MAX_VALUE_23_BIT_1;                                            // MAX_VALUE_23_BIT_1= 8388607.0;
    const double _Vexc = 5.00;                                                                     // Tension d'excitation

public:

    ADS1256(SPIClass* spi, uint8_t cs, uint8_t drdy,double Vexc, uint32_t SPI_Speed = 100000);     // Creation d'un contructeur 

    void setChannel(uint8_t pos, uint8_t neg);                                                     
    void begin();  
    void writeRegister(uint8_t reg, uint8_t value);                                                // FCT d'écriture dans un registre
    void sync();                                                                                   // FCT de synchronisation
    void select();                                                                                 // FCT por mettre ADS à l'écoute (LOW)
    void deselect();                                                                               // FCT pour mettre l'ADS  à l'état (HIGH) inactif

    int32_t readRaw();                                                                             // FCT pour lire la valeur Brute 

    double get_vref();                                                                             // tension de réference pour ADS 2.5V
};


#endif