/* Alusta asti kirjoitettu testiohjelma, jossa pyritään ohjaamaan anturia suoraan arduinon Wire.h-kirjaston kautta.
 * Jos tämä menetelmä toimii paremmin/antaa enemmän vapautta toimintaan kuin Sensirionin oma SensirionI2CStc3x.h, niin tästä luultavasti tulee oma kirjastonsa.
 * 
 * Tällä hetkellä komentojen lähetys ja raakadatan lukeminen toimii (toivottavasti)
 */
#include <Arduino.h>
#include <Wire.h>
#include <inttypes.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define i2caddr 0x29 //i2c osoite anturille oletuksena 0x29 (41)
#define WL_MAX_ATTEMPTS 4 //Maksimimäärä sallittuja yrityksiä wlanin yhdistämiselle

enum{
  WL_NOT_CONNECTED, //Nettiä ei yhdistetty eikä ole yritetty
  WL_CONNECT_SUCCESS, //Netti on yhdistetty
  WL_CONNECT_ERROR //Nettiä ei ole yhdistetty ja on yritetty
} WLAN_STATE;



//Wifi
char ssid[]="";
char pass[]="";

//MQTT
char broker[]="10.0.0.175";
int port = 1883;
char topic[]="test";

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  while(!Serial){
    delay(100);
  }

  Wire.begin();

  WLAN_STATE = WL_NOT_CONNECTED; //Aloitustilassa oletetaan wlan pois päältä

  /* Poistetaan CRC käytöstä */
  Wire.beginTransmission(i2caddr);
  Wire.write(0x37);Wire.write(0x68); //0x3768 disable CRC
  Wire.endTransmission();


  /* Self test */
  Serial.print("Itsetesti (00 == OK): ");
  Wire.beginTransmission(i2caddr);
  Wire.write(0x36);Wire.write(0x5B); //0x365B Self test
  Wire.endTransmission();

  delay(100); //Tarvittu viive (voi olla lopuksi pienempikin, 100 toimii nyt)

  /* Luetaan vastaus (00 == Kaikki OK) */
  Wire.requestFrom(i2caddr,2);
  while(Wire.available()) {
    Serial.print(Wire.read());
  }
  Serial.println();

  /* Asetetaan kaasu CO2 ilmassa 0-100% */
  Wire.beginTransmission(i2caddr);
  Wire.write(0x36);Wire.write(0x15); //0x3165 set binary gas
  Wire.write(0x00);Wire.write(0x01); //arg 0x0001 CO2 in air 0 to 100
  Wire.endTransmission();

  checkStateAndConnect();
  connectMQTT();

}

void checkStateAndConnect(){
  if(WLAN_STATE == WL_NOT_CONNECTED){
    enableWifi(); //Oletetaan, että netti löytyy sieltä mistä pitääkin ja yhdistetään
  }else if(WLAN_STATE == WL_CONNECT_ERROR){
    delay(5000); //Odotetaan pitempi aika jospa se netti tulisi sillä aikaa takaisin
    enableWifi();
  }
  
}

void enableWifi(){
  int attempts = 0;
  WiFi.disconnect(false);
  WiFi.mode(WIFI_STA);

  Serial.println("Käynnistetään wlan");
  WiFi.begin(ssid,pass);
  
  while(WiFi.status() != WL_CONNECTED){
     if(attempts >= WL_MAX_ATTEMPTS){ 
      WLAN_STATE = WL_CONNECT_ERROR;
      break;
    }
    delay(1000);
    Serial.print(".");
    ++attempts;
  }

  if(WLAN_STATE == WL_CONNECT_ERROR){
    Serial.print("Wlaniin yhdistäminen epäonnistui, yrityksiä: ");
    Serial.println(attempts);
  }else{
    Serial.println("Wlan yhdistetty onnistuneesti");
  }
 
}

void connectMQTT(){
  Serial.println("yhdistetään mqtt...");
  while(!client.connected()){
    Serial.print(client.connect("esp32"));
    Serial.println(client.state());
    delay(500);
  }
   
   Serial.println("mqtt yhdistetty");
}

void loop() {
  delay(2000);

  /* Mitataan kaasu */
  Wire.beginTransmission(i2caddr);
  Wire.write(0x36);Wire.write(0x39); //0x3639 measure gas concentration
  Wire.endTransmission();

  delay(100); //Tarvitaan viive ennen lukua (saattaa toimia pienemmälläkin)
  
  int i = 0;
  uint8_t bytes[2]={0,0}; //Voidaan laajentaa jos halutaan lukea myös lämpötila
  uint16_t result=0;
  
  /* Luetaan saadut tiedot */
  Wire.requestFrom(i2caddr, 2); //luetaan 2 tavua, tiedot kaasusta (ei lämpötilaa)
  while(Wire.available()){
    bytes[i] = Wire.read(); //Data tulee MSB ensin 
    ++i;
  }
  //liitetään molemmat arvot samaan
  result = ((uint16_t)bytes[0]<<8) | (uint16_t)bytes[1]; //rullataan ensimmäistä tavua vasemmalle 8 paikkaa ja sitten bittitason OR, jotta saadaan yksi 16 bittinen arvo.
  Serial.println(result);

  int len = snprintf(NULL,0,"%d",result); //Hankitaan mitatun datan pituus
  char* sResult = (char*)malloc(len*sizeof(char)+1); //Varataan muistia mjonolle jonka pituus on mittaustulos + NULL
  snprintf(sResult, len+1, "%d", result);//Kirjataan tulos varattuun merkkijonoon
 
  client.publish(topic,sResult);//Lähetetään mitattu tieto MQTT brokerille
  
  free(sResult);//Vapautetaan muisti

  
  //Tarkistusta varten
  /*Serial.print("\t");
  Serial.print(bytes[0]);
  Serial.print(" ");
  Serial.println(bytes[1]);*/

}
