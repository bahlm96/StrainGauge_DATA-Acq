
/**********************************Script écrit par : BALA Halim************************************ */
/**************************************Date : 17/02/2026********************************************* */
/*****************************Test pratique pour Stage : Développement d'une chaine d'acquisition*****/ 
/*Ce script permet de créer un fichier text sur une carte mémoire SD relié à un microcontroleur type ESP32 avec un protocole SPI et d'écrire sur cette carte SD*/


#include <Arduino.h>
#include <SD.h>
#include <SPI.h>


void setup(){

Serial.begin(115200); ///// aussi l'ajouter sur (platformio.ini) comme suite ---> monitor_speed = 115200
SD.begin(5);

if (!SD.begin(5)){ //// si t'arrive pas à lire sur la pin 5 la carte SD
Serial.println("erreur amigoooo ,,,, carte SD non reconnu!!!!!!!!!!!!!!!");  
  return;
} 
Serial.println("Carte SD it s OK");


File myfile = SD.open("/text.txt",FILE_WRITE); /// crée un fichier qui s'appelle text sur la carte SD et l'ouvrire en mode écriture

if(myfile){ //// si t'arrive à lire mon fichier donc affiche le text que j'ai écrit en dessous
  Serial.println("allo it s me ,,,, your carte it s okkkkkkkk");
   myfile.close(); //// très important de fermer le fichier après chaque écriture sinon ça va être toujour occupé
}
Serial.println("erreur !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");


}
void loop(){
/// j'ai rien ici pour le moment,,,,,,,,
}














