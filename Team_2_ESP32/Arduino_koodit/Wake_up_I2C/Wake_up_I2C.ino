#include <Arduino.h>
#include <Wire.h>

#define co2_addr 0x29 //i2c osoite co2 anturille, oletuksena 0x29 (41)

const int I2C_SDA_PIN = 6;
const int I2C_SCL_PIN = 7;
void setup() {
  Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(1000000);
  Wire.begin();
  Serial.begin(115200);

  iniliaze();

}


void iniliaze(){

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

void loop() {
  for(int x = 0; x < 10; x++ ){
    readCo2Sensor();
  }

  delay(1000);
  CO2_sleep();
  for(int x = 0; x < 10; x++ ){
    readCo2Sensor();
  }
  delay(1000);
  CO2_wakeup();
  delay(1000);
  iniliaze();
  delay(1000);
}



void CO2_sleep(){ //CO2 nukuttaminen
  Wire.beginTransmission(co2_addr);
  Wire.write(0x36);Wire.write(0x77); //Sleep tilan käyttöön otto
  Wire.endTransmission();
  Serial.println("Nukkuu");
}

void CO2_wakeup(){ //CO2 Herättäminen
    Wire.beginTransmission(co2_addr);
    Wire.endTransmission();
    Wire.requestFrom(co2_addr,7,true);
    Serial.println("Herää");

}




void readCo2Sensor(){
  /* Mitataan kaasu */
  Wire.beginTransmission(co2_addr);
  Wire.write(0x36);Wire.write(0x39); //0x3639 measure gas concentration
  Wire.endTransmission();

  delay(100); //Tarvitaan viive ennen lukua (saattaa toimia pienemmälläkin)
  
  int i = 0;
  uint8_t test[4]={0,0,0,0}; 
  uint16_t co2_result=0,temp_result=0;
  
  /* Luetaan saadut tiedot */
  Wire.requestFrom(co2_addr, 4); //luetaan 4 tavua, tiedot ensin kaasusta ja sitten lämpötilasta
  while(Wire.available()){
    test[i] = Wire.read(); //Data tulee MSB ensin 
    ++i;
  }
  //liitetään molemmat tavut yhdeksi arvoksi
  co2_result = ((uint16_t)test[0] << 8) | (uint16_t)test[1]; //rullataan ensimmäistä tavua vasemmalle 8 paikkaa ja sitten bittitason OR, jotta saadaan yksi 16 bittinen arvo.
  temp_result = ((uint16_t)test[2] << 8) | (uint16_t)test[3]; //sama mutta lämpötilalle
  
  //Tarkistusta varten
  Serial.print("Co2: ");
  Serial.print(co2_result);
  Serial.print("\t");
  Serial.print("Temp: ");
  Serial.println(temp_result);

}
