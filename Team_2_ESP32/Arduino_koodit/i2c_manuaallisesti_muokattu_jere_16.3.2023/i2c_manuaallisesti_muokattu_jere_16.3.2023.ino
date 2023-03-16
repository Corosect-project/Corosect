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
#define WL_MAX_ATTEMPTS 3 //Maksimimäärä sallittuja yrityksiä wlanin yhdistämiselle
#define MQTT_MAX_ATTEMPTS 3 //Maksimimäärä sallittuja yrityksiä mqtt yhdistämiselle
#define WL_CONNECT_TRY_TIME 1500 //Odotusaika per yritys (ms)
#define MQTT_CONNECT_TRY_TIME 1000 //sama mqtt
#define SAMPLES 10 //montako kertaa mitataan

/* Pinnit eivät voi olla oletuspinnit, koska pinniä 8 tarvitaan LEDin kanssa*/
const int I2C_SDA_PIN = 6;
const int I2C_SCL_PIN = 7;



enum{
    PROGRAM_START,
    WLAN_NOT_FOUND,
    WLAN_FOUND,
    WLAN_NOT_CONNECTED,
    WLAN_CONNECT_SUCCESS,
    WLAN_CONNECT_ERROR,
    MQTT_CONNECT_SUCCESS,
    MQTT_CONNECT_ERROR,
    MEASURING_DATA,
    SENDING_DATA,
    ALL_DONE
}PROGRAM_STATE;

enum LED_COLOR{
  WHITE,
  RED,
  GREEN,
  BLUE,
  YELLOW,
  PURPLE,
  ORANGE,
  BLACK
};

//Wifi
char ssid[]="panoulu";
char pass[]="";

//MQTT
char broker[]="100.64.254.11";
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

  PROGRAM_STATE = PROGRAM_START;
  Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(1000000);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  client.setServer(broker,port); //MQTT asetukset

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

  checkWifiAvailable();
  if(PROGRAM_STATE == WLAN_FOUND){
    Serial.println("Wifi löytyi");
  }else{
    Serial.println("Ei löytynyt");
  }
}

void setLED(LED_COLOR color){
  switch(color){
    case RED:
      neopixelWrite(RGB_BUILTIN,255,0,0);
      break;
    case BLUE:
      neopixelWrite(RGB_BUILTIN,0,0,255);
      break;
    case GREEN:
      neopixelWrite(RGB_BUILTIN,0,255,0);
      break;
    case WHITE:
      neopixelWrite(RGB_BUILTIN,255,255,255);
      break;
    case BLACK:
      neopixelWrite(RGB_BUILTIN,0,0,0);
      break;
    case PURPLE:
      neopixelWrite(RGB_BUILTIN,255,0,255);
      break;
    case YELLOW:
      neopixelWrite(RGB_BUILTIN,255,255,0);
      break;
    case ORANGE:
      neopixelWrite(RGB_BUILTIN,255,162,0);
      break;
    default:
    break;
  }  
}

void checkWifiAvailable(){
  bool flag = false;
  byte n = WiFi.scanNetworks();
  
  for(int i = 0; i < n; ++i){
    Serial.println(WiFi.SSID(i));
    if(WiFi.SSID(i).compareTo(ssid) == 0){ flag=true; break;}
  }
  PROGRAM_STATE = (flag) ? WLAN_FOUND : WLAN_NOT_FOUND;
}

void connectWifi(){
    PROGRAM_STATE = WLAN_NOT_CONNECTED;
    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid,pass);
    Serial.print("Aloitetaan yhteys");

    int attempts = 0;
    while(WiFi.status() != WL_CONNECTED){
      if(attempts >= WL_MAX_ATTEMPTS){
        PROGRAM_STATE = WLAN_CONNECT_ERROR;
        break;
      }
        
      Serial.print(".");
      
      delay(WL_CONNECT_TRY_TIME);
      ++attempts;
    }

    if(WiFi.status() == WL_CONNECTED){
      PROGRAM_STATE = WLAN_CONNECT_SUCCESS;
      Serial.println("Yhdistettiin!");
    }else{
      PROGRAM_STATE = WLAN_CONNECT_ERROR;
      Serial.print("Ei yhdistetty ");
      Serial.println(WiFi.status());
    }
  
  
}

void connectMQTT(){
  client.connect(devname);
  int attempts=0;

  Serial.println("aloitetaan mqtt");
  while(!client.connected()){
    if(attempts >= MQTT_MAX_ATTEMPTS){
      PROGRAM_STATE = MQTT_CONNECT_ERROR;
      break;
    }
    Serial.print(".");

    delay(1000);
    ++attempts;
  }

  if(client.connected()){
    PROGRAM_STATE = MQTT_CONNECT_SUCCESS;
    Serial.println("MQTT yhteys onnistui!");
  }else{
    PROGRAM_STATE = MQTT_CONNECT_ERROR;
    Serial.println("mqtt yhteys epäonnistui");
    goToSleep(1000);
  }
  
}

void goToSleep(int ms){
  PROGRAM_STATE = PROGRAM_START;
  disableWifi(); //varmaan turha jos nukkumistila kuitenkin sammuttaa wifin mutta onpa kuitenkin

  esp_sleep_enable_timer_wakeup(ms*1000); //ajastin käyttää aikaa mikrosekunneissa
  esp_deep_sleep_start();
    
  Serial.println("Krooh pyyh");
}

void disableWifi(){
  //WiFi pois päältä ja samalla katkeaa MQTT yhteys
  WiFi.disconnect(true); 
  WiFi.mode(WIFI_OFF);
  PROGRAM_STATE = PROGRAM_START;
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

  //Lähetetään nyt tiedot vaan suoraan brokerille
  sendResult(co2_result, co2_topic);
  sendResult(temp_result, temp_topic);
}

void sendResult(uint16_t val, char* topic){ 
  int len = snprintf(NULL,0,"%d",val); //Hankitaan mitatun datan pituus
  char* sResult = (char*)malloc((len+1)*sizeof(char)); //Varataan muistia mjonolle jonka pituus on mittaustulos + NULL
  snprintf(sResult, len+1, "%d", val);//Kirjataan tulos varattuun merkkijonoon

  client.publish(topic,sResult);//Lähetetään mitattu tieto MQTT brokerille
  
  free(sResult);//Vapautetaan muisti
}

void readResults(){
  PROGRAM_STATE = MEASURING_DATA;
  for(int i = 0; i<SAMPLES;++i){
    readCo2Sensor();
    delay(1000);
  }
  PROGRAM_STATE = ALL_DONE;
}

void CO2_sleep(){ //CO2 nukuttaminen
  Wire.beginTransmission(co2_addr);
  Wire.write(0x36);Wire.write(0x77); //Sleep tilan käyttöön otto
  Wire.endTransmission();
}

void CO2_wakeup(){ //CO2 Herättäminen
    Wire.beginTransmission(co2_addr);
    Wire.write(0x0); //Herätys komento
    Wire.endTransmission();
}

void loop() {
  delay(100);

  switch(PROGRAM_STATE){
    case PROGRAM_START: //Ollaan juuri käynnistetty tai palattu unesta, etsitään ensin wifi
        checkWifiAvailable();
        break;
    case WLAN_FOUND: //Wifi löytyi -> yritetään yhdistystä
        setLED(YELLOW);
        connectWifi();
        break;
    case WLAN_NOT_FOUND: //Wifiä ei löytynyt -> nukkumaan
        Serial.println("Nukutaan vielä pitemmin");
        setLED(RED);
        goToSleep(5000);
        break;
    case WLAN_CONNECT_SUCCESS: //Wifi yhteys onnistui -> yritetään mqtt yhteyttä
        setLED(GREEN);
        connectMQTT();
        break;
    case WLAN_CONNECT_ERROR: //Wifi yhteys epäonnistui -> nukkumaan
        Serial.println("Wifi yhteys epäonnistui");
        setLED(RED);
        goToSleep(5000);
        break;
    case MQTT_CONNECT_SUCCESS://mqtt yhteys onnistui -> luetaan mitaustulokset
        setLED(BLUE);
        readResults();
        break;
    case MQTT_CONNECT_ERROR: //mqtt yhteys epäonnistui -> nukkumaan
        setLED(RED);
        goToSleep(5000);
        break;
    case ALL_DONE: //mittaustulokset luettu ja lähetetty -> voidaan mennä lepotilaan uudestaan
        Serial.println("Mitattu, nukkumaan");
        setLED(BLACK);
        goToSleep(5000);
    default:
        //lole
        break;
  }  

}
