#include <Wire.h>
#include <stdio.h>
#include <iostream>
const int I2C_SDA_PIN = 6, I2C_SCL_PIN = 7;
#define co2_addr 0x29 //i2c osoite co2 anturille, oletuksena 0x29 (41)
int x = 0;
void setup() {
  Serial.begin(115200);
  Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(1000000);
  Wire.begin();

   /* Poistetaan CRC käytöstä */
  Wire.beginTransmission(co2_addr);
  Wire.write(0x37);Wire.write(0x68); //0x3768 disable CRC
  Wire.endTransmission();

  /* Self test */
  Serial.print("Itsetesti (00 == OK): ");
  Wire.beginTransmission(co2_addr);
  Wire.write(0x36);Wire.write(0x5B); //0x365B Self test
  Wire.endTransmission();

  delay(100); //Tarvittu viive (voi olla lopuksi pienempikin, 100 toimii nyt)

  /* Luetaan vastaus (00 == Kaikki OK) */
  Wire.requestFrom(co2_addr,2);
  while(Wire.available()) {
    Serial.print(Wire.read());
  }
  Serial.println();

  /* Asetetaan kaasu CO2 ilmassa 0-100% */
  Wire.beginTransmission(co2_addr);
  Wire.write(0x36);Wire.write(0x15); //0x3165 set binary gas
  Wire.write(0x00);Wire.write(0x01); //arg 0x0001 CO2 in air 0 to 100
  Wire.endTransmission();
}

void readCo2Sensor(){
  /* Mitataan kaasu */
  Wire.beginTransmission(co2_addr);
  Wire.write(0x36);Wire.write(0x39); //0x3639 measure gas concentration
  Wire.endTransmission();

  delay(100); //Tarvitaan viive ennen lukua (saattaa toimia pienemmälläkin)
  
  Wire.requestFrom(co2_addr, 6); //luetaan 2 tavua, tiedot kaasusta (ei lämpötilaa)
  while(Wire.available()){
    Serial.print(byte(Wire.read()));
    Serial.print(" ");
  }
  Serial.println();
}



void CO2_sleep(){ //CO2 nukuttaminen
  Wire.beginTransmission(co2_addr);
  Wire.write(0x36);Wire.write(0x77); //Sleep tilan käyttöön otto
  Wire.endTransmission();
}

void CO2_wakeup(){ //CO2 Herättäminen
    char number [33];
    itoa(x, number, 16);
    Serial.println(String("Numero: ") + number);
    Wire.beginTransmission(co2_addr);
    Wire.write(number); //Herätys komento
    Wire.endTransmission();

    x++;
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("sensori normaalisti");
  readCo2Sensor();
  delay(1000);
  Serial.println("sensori nukkuu");
  CO2_sleep();
  readCo2Sensor();
  delay(1300);
  Serial.println("sensori herätetään");
  CO2_wakeup();
  delay(1300);
  readCo2Sensor();
  delay(1000);
}

