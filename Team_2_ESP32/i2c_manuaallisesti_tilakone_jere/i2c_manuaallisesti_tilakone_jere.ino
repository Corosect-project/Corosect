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
#define WL_CONNECT_TRY_TIME 1000 //Aika joka yritetään yhdistää wlaninn (ms)
#define MQTT_CONNECT_TRY_TIME 1000 //sama mqtt

//Reaaliaika Kellon poistaminen alkaa 
#define RTC_CNTL_TIMER_XTL_OFF 0x60008000   //Pointteri osoittamaan rekisteriin RTC_CNTL_TIME_UPDATE_REG 
//Low-Power Management low address 0x6000_8000 + 0x000C
volatile uint32_t* pointer = (volatile uint32_t*)(RTC_CNTL_TIMER_XTL_OFF+0x000C);
//Reaaliaika Kellon poistaminen loppuu 



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
  Wire.begin();

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
    goToSleep(1000);
  }

//  checkStateAndConnect();
  //connectMQTT();
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
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid,pass);

    int startt = millis();
    int endt = startt;

    while((endt - startt) <= WL_CONNECT_TRY_TIME){ //yritetään yhteyttä 1 sekunnin ajan
      if(WiFi.status() == WL_CONNECTED){
        PROGRAM_STATE = WLAN_CONNECT_SUCCESS;
        break;
      }else{}
      delay(10);
      endt = millis();
    }

    if(PROGRAM_STATE == WLAN_CONNECT_SUCCESS){
      Serial.println("Yhdistettiin!");
    }else{
      PROGRAM_STATE = WLAN_CONNECT_ERROR;
      Serial.println("Ei yhdistetty");
      goToSleep(1000);
    }
  
  
}

void connectMQTT(){
  client.connect(devname);
  int startt = millis();
  int endt = startt;
  Serial.println(startt);
  Serial.println(endt);


    while((endt - startt) <= MQTT_CONNECT_TRY_TIME){ //yritetään yhteyttä 1 sekunnin ajan
      if(client.connected()){
        PROGRAM_STATE = MQTT_CONNECT_SUCCESS;
        break;
     }
      endt = millis();

    delay(10);
  }

  if(PROGRAM_STATE == MQTT_CONNECT_SUCCESS){
    Serial.println("MQTT yhteys onnistui!");
  }else{
    PROGRAM_STATE = MQTT_CONNECT_ERROR;
    Serial.println("mqtt yhteys epäonnistui");
    goToSleep(1000);
  }
  
}

void goToSleep(int ms){
  PROGRAM_STATE = PROGRAM_START;
  disableWifi();
  Serial.println("Krooh pyyh");
  delay(ms);
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
  Serial.print("\tTemp: ");
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
  for(int i = 0; i<10;++i){
    readCo2Sensor();
    delay(100);
  }
  PROGRAM_STATE = ALL_DONE;
}

void handleState(){
  switch(PROGRAM_STATE){
    case PROGRAM_START:
        neopixelWrite(RGB_BUILTIN,255,255,255);
        checkWifiAvailable();
        break;
    case WLAN_FOUND:
        neopixelWrite(RGB_BUILTIN,255,255,0);
        connectWifi();
        break;
    case WLAN_CONNECT_SUCCESS:
        neopixelWrite(RGB_BUILTIN,0,0,255);
        connectMQTT();
        break;
    case MQTT_CONNECT_SUCCESS:
        neopixelWrite(RGB_BUILTIN,0,255,0); 
        readResults();
        break;
    case ALL_DONE:
        Serial.println("Mitattu, nukkumaan");
        neopixelWrite(RGB_BUILTIN,255,0,0); //Punainen
        WiFi.setSleep(true);

        void sleepi_aika() { //Demo sleep tilasta
        esp_sleep_enable_timer_wakeup(5 * 1000000ULL); //Ajastin 5 sekunttia herätys      
        esp_deep_sleep_start(); //Syvä sleep tila aktivointi
        esp_light_sleep_start(); //Kevyt sleep tila aktivointi
        }
        
        goToSleep(5000);
        WiFi.setSleep(false);
        
    default:
        //lole
        break;
  }  
}

void loop() {
  delay(1000);

  handleState();

}