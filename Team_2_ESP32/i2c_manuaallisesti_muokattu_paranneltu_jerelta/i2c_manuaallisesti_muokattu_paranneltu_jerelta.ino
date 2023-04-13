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
#define MQTT_MAX_ATTEMPTS 5 //Maksimimäärä sallittuja yrityksiä wlanin yhdistämiselle




//RTC kellon poistaminen alkaa
/*
//volatile uint32_t* RTC_CNTL_TIME_UPDATE_REG =  (uint32_t*)0x000C;  //Poistetaan RTC kello käytöstä koska sitä ei tarvita

//RTC_CNTL_TIME_UPDATE_REG = (uint32_t)0x28; //Poistetaan RTC kello käytöstä koska sitä ei tarvita 
#define RTC_CNTL_TIME_UPDATE_REG_ADDR       0x000C //RTC kellon osoite
#define READ_RTC_CNTL_TIME_UPDATE_REG()     (*((volatile uint32_t *)RTC_CNTL_TIME_UPDATE_REG_ADDR)) //Luetaan rtc kellon tila
#define WRITE_LRTC_CNTL_TIME_UPDATE_REG()   (*((volatile uint32_t *)RTC_CNTL_TIME_UPDATE_REG_ADDR) = (0x28)) //Kirjoitetaan rtc kello pois päältä
*/
#define RTC_CNTL_TIMER_XTL_OFF 0x60008000   //Pointteri osoittamaan rekisteriin RTC_CNTL_TIME_UPDATE_REG 
//Low-Power Management low address 0x6000_8000 + 0x000C
volatile uint32_t* pointer = (volatile uint32_t*)(RTC_CNTL_TIMER_XTL_OFF+0x000C);
//Kellon poistaminen loppuu 

enum{
  WLAN_NOT_CONNECTED, //Nettiä ei yhdistetty eikä ole yritetty
  WLAN_CONNECT_SUCCESS, //Netti on yhdistetty
  WLAN_CONNECT_ERROR //Nettiä ei ole yhdistetty ja on yritetty
} WLAN_STATE;

enum{
  MQTT_NOT_CONNECTED,
  MQTT_CONNECT_SUCCESS,
  MQTT_CONNECT_ERROR
} MQTT_STATE;

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
  Serial.println("Tama on osoitteen arvo: "+ (String)* pointer); //Luetaan rtc kellon tila


  Wire.begin();

  WLAN_STATE = WLAN_NOT_CONNECTED; //Aloitustilassa oletetaan wlan pois päältä
  MQTT_STATE = MQTT_NOT_CONNECTED;

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



void sleepstate_ON_Wifi(){ //Laitetaan Wifin virransäästötila päälle ja odotetaan sekuntti
    WiFi.setSleep(true); //WiFi nukkumistila
    while(WLAN_STATE == 0 || 2){
      delay(1000);
      Serial.println("Ollaan WiFi sleepissä");
      WLAN_STATE = WLAN_STATE;
    }
    WiFi.setSleep(false);
}

void checkStateAndConnect(){
  switch(WLAN_STATE){
    case WLAN_NOT_CONNECTED: //ei ongelmia edellisellä kerralla
      //sleepstate_ON_Wifi();
      enableWifi(); //oletetaan että wifi löytyy 
      break;
    case WLAN_CONNECT_ERROR: //oli ongelmia edellisellä kerralla
      //sleepstate_ON_Wifi();
      enableWifi(); //yritetään vain uudelleen tässäkin, virheenkäsittely tähän
      break;
    case WLAN_CONNECT_SUCCESS:
      Serial.println("Verkkoyhteys on pystyssä");
      break;
  }
}

void checkMQTT(){
  switch(MQTT_STATE){
    case MQTT_NOT_CONNECTED: //Ei ollut ongelmia edellisellä kerralla
      connectMQTT(); //yhdistetään
      break;
    case MQTT_CONNECT_ERROR: //Oli ongelmia
      connectMQTT(); //yritetään vain uudelleenyhdistämistä
      break;
    case MQTT_CONNECT_SUCCESS:
      Serial.println("MQTT yhteys on pystyssä");
      break;
  }
}

void disableWifi(){
  //WiFi pois päältä ja samalla katkeaa MQTT yhteys
  WiFi.disconnect(true); 
  WiFi.mode(WIFI_OFF);
  WLAN_STATE = WLAN_NOT_CONNECTED;
  MQTT_STATE = MQTT_NOT_CONNECTED;
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
    delay(500);
    ++attempts;
  }

  if(WLAN_STATE == WLAN_CONNECT_ERROR){ /*Yhdistäminen epäonnistui, kerrotaan yritysten määrä */
    Serial.print("Wlaniin yhdistäminen epäonnistui, yrityksiä: ");
    Serial.println(attempts);
  }else{ /*Toinen vaihtoehto on, että yhdistäminen onnistui, joten asetetaan tilaksi WLAN_CONNECT_SUCCESS */
    WLAN_STATE = WLAN_CONNECT_SUCCESS;
    Serial.println("Wlan yhdistetty onnistuneesti");
    checkMQTT();//Yhdistäminen onnistui, aloitetaan mqtt
  }
}

void connectMQTT(){
  int attempts = 0;
  client.setServer(broker,port);
  Serial.println("yhdistetään mqtt");
  /* Tarvitsee myös jonkinlaisen tarkistuksen, turha pitää wlania päällä jos mqtt ei yhdistä */
  while(!client.connected()){
    if(attempts >= MQTT_MAX_ATTEMPTS){
      MQTT_STATE = MQTT_CONNECT_ERROR;
      break;
    }
    Serial.print(client.connect(devname));
    Serial.println(client.state());
    delay(500);
    ++attempts;
  }

  if(MQTT_STATE == MQTT_CONNECT_ERROR){
    Serial.print("MQTT yhdistäminen epäonnistui, yrityksiä: ");
    Serial.println(attempts);
  }else{
    Serial.println("MQTT yhdistetty onnistuneesti");
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
  co2_result = ((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]; //rullataan ensimmäistä tavua vasemmalle 8 paikkaa ja sitten bittitason OR, jotta saadaan yksi 16 bittinen arvo.
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

  //Varmistetaan että saadaan MQTT yhteys
  if(!client.connected()){
    checkMQTT();
  }else{
    client.publish(topic,sResult);//Lähetetään mitattu tieto MQTT brokerille
  }
  
  free(sResult);//Vapautetaan muisti
}

void loop() {
  delay(100);
  checkStateAndConnect(); //Yhdistetään wifi ja MQTT
  for(int i = 0; i < 10; ++i){ //Luetaan ja lähetetään nyt testiksi 10 näytettä ihan vaan suoraan
    readCo2Sensor();
    delay(500);
  }
  Serial.println("Lähetys ohi");

  disableWifi(); //wifi ja MQTT yhteys pois päältä kun on mitattu ja lähetetty
  delay(5000);
  
  

}