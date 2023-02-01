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

#define co2_addr 0x29 //i2c osoite co2 anturille, oletuksena 0x29 (41)
#define WL_MAX_ATTEMPTS 5 //Maksimimäärä sallittuja yrityksiä wlanin yhdistämiselle

enum{
  WLAN_NOT_CONNECTED, //Nettiä ei yhdistetty eikä ole yritetty
  WLAN_CONNECT_SUCCESS, //Netti on yhdistetty
  WLAN_CONNECT_ERROR //Nettiä ei ole yhdistetty ja on yritetty
} WLAN_STATE;

//Wifi
char ssid[]="panoulu";
char pass[]="";

//MQTT
char broker[]="100.71.190.209";
int port = 1883;
char devname[] = "esp32";

char co2_topic[]="co2";
char temp_topic[]="temp";

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  while(!Serial){
    delay(100);
  }

  Wire.begin();

  WLAN_STATE = WLAN_NOT_CONNECTED; //Aloitustilassa oletetaan wlan pois päältä

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

  checkStateAndConnect();
  //connectMQTT();
}

void checkStateAndConnect(){
  if(WLAN_STATE == WLAN_NOT_CONNECTED){
    enableWifi(); //Oletetaan, että netti löytyy sieltä mistä pitääkin ja yhdistetään
  }else if(WLAN_STATE == WLAN_CONNECT_ERROR){
    delay(5000); //Odotetaan pitempi aika jospa se netti tulisi sillä aikaa takaisin
    enableWifi();
  }
}

void disableWifi(){
  //WiFi pois päältä ja samalla katkeaa MQTT yhteys
  WiFi.disconnect(true); 
  WiFi.mode(WIFI_OFF);
  WLAN_STATE = WLAN_NOT_CONNECTED;  
}

void enableWifi(){
  int attempts = 0;
  WiFi.disconnect(false);
  WiFi.mode(WIFI_STA);

  Serial.println("Käynnistetään wlan");
  WiFi.begin(ssid,pass);

  /* En ole täysin tyytyväinen tähän, sisältää luultavasti jonkinlaisen virheen/virheitä.
   * Vaatii siis varmaan uudelleenkirjoituksen joskus pian(tm)*/
   //TODO: keksi järkevä tapa hoitaa tämä
  while(WiFi.status() != WL_CONNECTED){ 
     if(attempts >= WL_MAX_ATTEMPTS){ //Yritysten määrä ylittyi, poistutaan loopista ja asetetaan tilaksi WLAN_CONNECT_ERROR
      WLAN_STATE = WLAN_CONNECT_ERROR;
      break;
    }
    Serial.print(".");
    delay(1000);
    ++attempts;
  }

  if(WLAN_STATE == WLAN_CONNECT_ERROR){ //Yhdistäminen epäonnistui, kerrotaan yritysten määrä
    Serial.print("Wlaniin yhdistäminen epäonnistui, yrityksiä: ");
    Serial.println(attempts);
  }else{ //Toinen vaihtoehto on, että yhdistäminen onnistui, joten asetetaan tilaksi WLAN_CONNECT_SUCCESS
    WLAN_STATE = WLAN_CONNECT_SUCCESS;
    Serial.println("Wlan yhdistetty onnistuneesti");
    connectMQTT();//Yhdistäminen onnistui, aloitetaan mqtt
  }
}

void connectMQTT(){
  client.setServer(broker,port);
  Serial.println("yhdistetään mqtt...");
  /* Tarvitsee myös jonkinlaisen tarkistuksen, turha pitää wlania päällä jos mqtt ei yhdistä */
  while(!client.connected()){ //TODO: kunnon tarkistus tähän
    Serial.print(client.connect(devname));
    Serial.println(client.state());
    delay(1000);
  }
}

void readCo2Sensor(){
  /* Mitataan kaasu */
  Wire.beginTransmission(co2_addr);
  Wire.write(0x36);Wire.write(0x39); //0x3639 measure gas concentration
  Wire.endTransmission();

  delay(100); //Tarvitaan viive ennen lukua (saattaa toimia pienemmälläkin)
  
  int i = 0;
  uint8_t bytes[4]={0,0,0,0}; 
  uint16_t co2_result=0,temp_result=0;
  
  /* Luetaan saadut tiedot */
  Wire.requestFrom(co2_addr, 4); //luetaan 4 tavua, tiedot ensin kaasusta ja sitten lämpötilasta
  while(Wire.available()){
    bytes[i] = Wire.read(); //Data tulee MSB ensin 
    ++i;
  }
  //liitetään molemmat tavut yhdeksi arvoksi
  co2_result = ((uint16_t)bytes[0]<<8) | (uint16_t)bytes[1]; //rullataan ensimmäistä tavua vasemmalle 8 paikkaa ja sitten bittitason OR, jotta saadaan yksi 16 bittinen arvo.
  temp_result = ((uint16_t)bytes[2] << 8) | (uint16_t)bytes[3]; //sama mutta lämpötilalle
  
  //Tarkistusta varten
  Serial.print("Co2: ");
  Serial.print(co2_result);
  Serial.print("\t");
  Serial.print("Temp: ");
  Serial.println(temp_result);

  //Lähetetään nyt tiedot vaan suoraan brokerille
  sendResult(co2_result, co2_topic);
  sendResult(temp_result, temp_topic);
}

void sendResult(uint16_t val, char* topic){ 
  int len = snprintf(NULL,0,"%d",val); //Hankitaan mitatun datan pituus
  char* sResult = (char*)malloc((len+1)*sizeof(char)); //Varataan muistia mjonolle jonka pituus on mittaustulos + NULL
  snprintf(sResult, len+1, "%d", val);//Kirjataan tulos varattuun merkkijonoon

  if(!client.connected()){ //TODO: oikea virheenkäsittely eikä raakaa voimaa
    connectMQTT();
  }
 
  client.publish(topic,sResult);//Lähetetään mitattu tieto MQTT brokerille
  
  free(sResult);//Vapautetaan muisti
}

void loop() {
  delay(2000);
  readCo2Sensor();

}
