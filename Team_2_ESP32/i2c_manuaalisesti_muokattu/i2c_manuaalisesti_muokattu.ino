/* Alusta asti kirjoitettu testiohjelma, jossa pyritään ohjaamaan anturia suoraan arduinon Wire.h-kirjaston kautta.
 * Jos tämä menetelmä toimii paremmin/antaa enemmän vapautta toimintaan kuin Sensirionin oma SensirionI2CStc3x.h, niin tästä luultavasti tulee oma kirjastonsa.
 * 
 * Tällä hetkellä komentojen lähetys ja raakadatan lukeminen toimii (toivottavasti)
 */
#include <Arduino.h>
#include <Wire.h>

int i2caddr = 0x29; //i2c osoite anturille oletuksena 0x29 (41)
bool muuttuja = true; //Muuttuva arvo i2c
byte arvo1, arvo2; //Bitti arvot tallennus
void setup() {
  Serial.begin(115200);
  while(!Serial){
    delay(100);
  }

  Wire.begin();
  Wire.setClock(1000000); //Asetetaan fast mode plus käyttöön (Nopein lukeminen)


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
    Serial.println(Wire.read());
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
  Wire.requestFrom(i2caddr, 6); //luetaan 2 tavua, tiedot kaasusta (ei lämpötilaa)
  while(Wire.available()){
    //tallennaluku(); //Tallennetaan luku
    Serial.print(byte(Wire.read()));
    Serial.print(" ");
  }
  Serial.println();
  //debug_luku(); //Luetaan luku



}


void unitila(){ //Unitila Co2 sensorille
  Wire.beginTransmission(i2caddr);
  Wire.write(0x36); //Enter Sleep mode 0x3677
  Wire.write(0x77);
  Wire.endTransmission();
}

void poistu_unitilasta(){ //Poistutaan unitilasta Co2 Sensori
  Wire.beginTransmission(i2caddr);
  Wire.write(0x0); //Poistutaan unitilasta
  Wire.endTransmission();
}

void tallennaluku(){ //Tallennetaan arvot
  switch(muuttuja){
      case true:
        arvo1 = Wire.read();
        break;
      case false:
        arvo2 = Wire.read();
        break;
    }
    muuttuja = !muuttuja;
}

void debug_luku(){ //Tulostetaan tallennetut arvot
  Serial.print(arvo1);
  Serial.print(" ");
  Serial.println(arvo2);
}
