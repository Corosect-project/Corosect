/* Alusta asti kirjoitettu testiohjelma, jossa pyritään ohjaamaan anturia suoraan arduinon Wire.h-kirjaston kautta.
 * Jos tämä menetelmä toimii paremmin/antaa enemmän vapautta toimintaan kuin Sensirionin oma SensirionI2CStc3x.h, niin tästä luultavasti tulee oma kirjastonsa.
 * 
 * Tällä hetkellä komentojen lähetys ja raakadatan lukeminen toimii (toivottavasti)
 */
#include <Arduino.h>
#include <Wire.h>

#define i2caddr 0x29 //i2c osoite anturille oletuksena 0x29 (41)


void setup() {
  Serial.begin(115200);
  while(!Serial){
    delay(100);
  }

  Wire.begin();

  /* Poistetaan CRC käytöstä */
  Wire.beginTransmission(i2caddr);
  Wire.write(0x37); //0x3768 disable CRC
  Wire.write(0x68);
  Wire.endTransmission();


  /* Self test */
  Wire.beginTransmission(i2caddr);
  Wire.write(0x36); //0x365B Self test
  Wire.write(0x5B);
  Wire.endTransmission();

  delay(100); //Tarvittu viive (voi olla lopuksi pienempikin, 100 toimii nyt)

  /* Luetaan vastaus (00 == Kaikki OK) */
  Wire.requestFrom(0x29,2);
  while(Wire.available()) {
    Serial.print(Wire.read());
  }

  /* Asetetaan kaasu CO2 ilmassa 0-100% */
  Wire.beginTransmission(i2caddr);
  Wire.write(0x36); //0x3165 set binary gas
  Wire.write(0x15);

  Wire.write(0x00); //arg 0x0001 CO2 in air 0 to 100
  Wire.write(0x01);
  Wire.endTransmission();

}

void loop() {
  delay(2000);

  /* Mitataan kaasu */
  Wire.beginTransmission(i2caddr);
  Wire.write(0x36); //0x3639 measure gas concentration
  Wire.write(0x39);
  Wire.endTransmission();

  delay(100); //Tarvitaan viive ennen lukua (saattaa toimia pienemmälläkin)

  /* Luetaan saadut tiedot */
  Wire.requestFrom(0x29, 2); //luetaan 2 tavua, tiedot kaasusta (ei lämpötilaa)
  while(Wire.available()){
    Serial.print(byte(Wire.read()));
    Serial.print(" ");
  }
  Serial.println();

}
