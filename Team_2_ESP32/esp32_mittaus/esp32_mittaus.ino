//Johdettu Stc3x kirjaston "exampleUsage" esimerkistä
#include <Arduino.h>
#include <SensirionI2CStc3x.h>
#include <Wire.h>
#include <inttypes.h>
#include "driver/adc.h"

SensirionI2CStc3x stc3x;

void setup() {
  Serial.begin(115200);
  while(!Serial){
    delay(100);
  }

  Wire.begin();

  uint16_t error;
  char errorMessage[256];


 // Wire.beginTransmission(0x29);   //STC3X osoite 0x29 (41)
  
 /* Poistetaan CRC käytöstä
   * Luultavasti tässä käyttötarkoituksessa turha ja saadaan vähennettyä
   * virran kulutusta */
   //Wire.write(0x3768); //Komento (disable CRC)
  // Wire.write(0x37);
   //Wire.write(0x68);

   //Wire.endTransmission();



   stc3x.begin(Wire);
   Wire.beginTransmission();
   Wire.write(0x37);
   Wire.write(0x68);
   Wire.endTransmission();


  //Suorittaa testin käynnistyksen yhteydessä
  uint16_t selfTestOutput;
  error = stc3x.selfTest(selfTestOutput);
  if(error){
    Serial.print("Virhe selfTest() suorituksessa: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }else{
    char buff[32];
    sprintf(buff, "SelfTest: 0x%04x (OK = 0x0000)",selfTestOutput);
    Serial.println(buff);
    
  }

 

  //Asetetaan kaasuksi CO2 ilmassa 0-100%
  error = stc3x.setBinaryGas(0x0001);
  if(error){
    Serial.print("Virhe kaasun asetuksessa: ");
    errorToString(error, errorMessage,256);
    Serial.println(errorMessage);
  }else{
    Serial.println("Kaasuasetus: CO2 ilmassa 0-100% (0x0001)");
  }



  

}

void measureGas(){
  //Muuttujat mitatuille arvoille
  uint16_t gasTicks;
  uint16_t temperatureTicks;

  uint16_t error;
  char errorMessage[256];

  error = stc3x.measureGasConcentration(gasTicks, temperatureTicks);
  if(error){
    Serial.print("Virhe kaasun mittauksessa: ");
    errorToString(error,errorMessage,256);
    Serial.println(errorMessage);
  }else{
    //Kaavat STC31 datasheetistä
    Serial.print("GasConcentration:");
    Serial.print(100 * ((float)gasTicks - 16384.0) / 32768.0);
    Serial.print("\t");
    Serial.print("Temp:");
    Serial.println((float)temperatureTicks / 200.0);
  }
  
}

void loop() {
  delay(2000);

  measureGas();
}
