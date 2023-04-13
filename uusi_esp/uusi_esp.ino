#include "Wire.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "SparkFun_STC3x_Arduino_Library.h"
#include <RunningMedian.h>
//Virrankulutuksen optimointia varten:
#include "driver/adc.h"


//HAVAINNOLLISTAMISTA VARTEN...
int keskiledPIN=13;

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//TIETOLIIKENNETOIMINNOT:
///////muista piilottaa salasanat jos tarpeen!!!
char ssid[] = "toukkapurkki";        // your network SSID (name)
char pass[] = "biomassa";    // your network password (use for WPA, or use as key for WEP)
//char ssid[] = "panoulu";        // your network SSID (name)
//char pass[] = "";    // your network password (use for WPA, or use as key for WEP)
WiFiClient espClient;
PubSubClient client(espClient); 

//Yhteysasetuksia:
//Huom; tässä kohdassa tulee olla oman Rasperryn IP-osoite. Konfiguraatiot tulee tehdä siten että 
//arduino pystyy löytämään MQTT-yhteyden ilman lisätyötä pilottikohteessa (kaikki asetukset ja salasanat ja muut suoraan tähän koodiin)
char broker[] = "192.168.2.150";  //Toukkalinna
int port=1883;

char temp_topic[]="temp";
char humidity_topic[]="humidity";
char co2_topic[]="co2";
char moisture_topic[]="moisture";
char nh3_topic[]="nh3";
//ja muistetaan että kuudes arvo on pH-arvo, joka tulee suoraan toisesta systeemistä (kamera jne...)

STC3x co2_sensori;
//Globaaleja muuttujia
float temp_raw;
float humidity_raw;
float co2_raw;
float moisture_raw;
float nh3_raw;

//Mittaustietojen suodatuksia varten...
int naytejonon_pituus=5;

RunningMedian temp_naytejono=RunningMedian(naytejonon_pituus);
RunningMedian humidity_naytejono=RunningMedian(naytejonon_pituus);
RunningMedian co2_naytejono=RunningMedian(naytejonon_pituus);
RunningMedian moisture_naytejono=RunningMedian(naytejonon_pituus);
RunningMedian nh3_naytejono=RunningMedian(naytejonon_pituus);

//Määritellään montako toistonäytettä sensorilta otetaan yhdellä kertaa (mediaani tai keskiarvoistus...)
int mittauksen_naytemaara=10;
//Määritellään ajoituksia varten viiveitä (ms)
int pikkuviive=1;
int perusviive=5;
int sekuntiviive=1000;
int viisiviive=5000;
//Lähetysvälit (huomaa virrankulutusnäkökulmat)
int tietojen_lahetysvali=5000;
long viimeksi_tehdyn_lahetyksen_aikaleima=0;
long aikaleima_juuri_nyt; 
long aikaero_viimeisimpaan_lahetykseen;

int wlanLED=13;

void setup(){

  pinMode(keskiledPIN,OUTPUT);

  Serial.begin(115200); 
  Serial.println("Aloitetaan...");
  while(!Serial){} // Odotellaan sarjaliikennettä...

  //CO2-anturiasioita:
  Wire.begin();
  delay(50);
  if (co2_sensori.begin() == false)
  {Serial.println("Sensoria ei löytynyt, mutta jatketaan silti...");}
  else{Serial.println("Sensorihan se täältä löytyi!!! Tadaa!!!");}
  if (co2_sensori.setBinaryGas(STC3X_BINARY_GAS_CO2_AIR_100) == false) // Asetetaan sensorin antamaksi dataksi 0-100% CO2 ilmassa
  {Serial.println("Binäärikaasuasetukset eivät onnistuneet, mutta jatketaan silti...");}
  else{Serial.println("Binäärikaasuasetukset onnistuivat!");}
  
  //WIFI-asiat
  enableWiFi();

}

float mittaa_temp(){
  Serial.println("");
  Serial.print("Temp:n mittaus alkaa...");
  for (int i=0;i<=mittauksen_naytemaara;i++){
    float temp_raw = random(20,30);
    temp_naytejono.add(temp_raw);
    delay(pikkuviive);
  }
  float temp_lopullinen=temp_naytejono.getMedian();
  Serial.print("...Temp:n mittaus paattyy!");
  return temp_lopullinen;
}

float mittaa_humidity(){
  Serial.println("");
  Serial.print("Humidity:n mittaus alkaa...");
  for (int i=0;i<=mittauksen_naytemaara;i++){
    float humidity_raw = random(50,100);
    humidity_naytejono.add(humidity_raw);
    delay(pikkuviive);
  }
  float humidity_lopullinen=humidity_naytejono.getMedian();
  Serial.print("...Humidity:n mittaus paattyy!");
  return humidity_lopullinen;
}

float mittaa_co2(){
  Serial.println("");
  Serial.print("CO2:n mittaus alkaa...");
  for (int i=0;i<=mittauksen_naytemaara;i++){
    float co2_raw = random(100,1000);
    co2_naytejono.add(co2_raw);
    delay(pikkuviive);
  }
  float co2_lopullinen=co2_naytejono.getMedian();
  Serial.print("...CO2:n mittaus paattyy!");
  return co2_lopullinen;
}

float mittaa_moisture(){
  Serial.println("");
  Serial.print("Moisture:n mittaus alkaa...");
  for (int i=0;i<=mittauksen_naytemaara;i++){
    float moisture_raw = random(0,100);
    //Serial.println(co2_raw);
    moisture_naytejono.add(moisture_raw);
    delay(pikkuviive);
  }
  float moisture_lopullinen=moisture_naytejono.getMedian();
  Serial.print("...Moisture:n mittaus paattyy!");
  return moisture_lopullinen;
}

float mittaa_nh3(){
  Serial.println("");
  Serial.print("NH3:n mittaus alkaa...");
  for (int i=0;i<=mittauksen_naytemaara;i++){
    float nh3_raw = random(0,1000);
    nh3_naytejono.add(nh3_raw);
    delay(pikkuviive);
  }
  float nh3_lopullinen=nh3_naytejono.getMedian();
  Serial.print("...NH3:n mittaus paattyy!");
  return nh3_lopullinen;
}


//Maaritetaan lahetystoiminto:
void laheta_tieto(float lukema_raw,char lahetys_topic[]){
  
  //Seuraavat tuleee olla kommentoituna mikäli WIFIä ei ole käytössä...(testausvaiheet...)
  //char lahetys_topic_c[];
  //lahetys_topic_c="moi"; //lahetys_topic;
  char lukema_raw_s[10];
  dtostrf(lukema_raw,6,2,lukema_raw_s);
  //Serial.println(lukema_raw_s);
  //delay(perusviive);
  client.publish(lahetys_topic,lukema_raw_s);
  delay(perusviive);
  //mqttClient.print(lukema_raw);
  delay(perusviive);
  //mqttClient.endMessage();
  delay(perusviive);
  
  Serial.println("");
  Serial.print(lukema_raw);
  Serial.print("--->");
  Serial.print(lahetys_topic);
  delay(perusviive);
}

int tarkista_yhteys_ja_nosta_ylos_jos_tarvetta(char ssid[],char pass[],char broker[],int port){ 
  Serial.println("Yhteystiedot:");
  Serial.print("Verkko, broker ja portti: ");
  Serial.print(ssid);
  Serial.print(", ");
  Serial.print(broker);
  Serial.print(" : ");
  Serial.print(port);
  Serial.println("");
  Serial.println("...tutkitaan seuraavaksi...");
  delay(perusviive);
  //Lähdetään oletukseesta että kaikki on kunnossa:  
  int wifi_ja_mqtt_kunto=1;

  if(WiFi.status()!=3){   
    wifi_ja_mqtt_kunto=0;
      for (int yritysnro=1;yritysnro<=5;yritysnro++){
        if (WiFi.begin(ssid,pass)==WL_CONNECTED){
          Serial.println("WLAN-yhteys saatiin kuntoon!");
          wifi_ja_mqtt_kunto=1;
        }  
      }
  
      if (wifi_ja_mqtt_kunto==0){    
        Serial.print("WLAN-yhteydessa on probleemia...vaikka yritettiin uudestaan: ");
        Serial.print("montako?");
        Serial.println(" kertaa!");
      }
      
  }
  
  Serial.print("Yritetään seuraavaksi yhdistää MQTT -välittäjälle: ");
           
    //Tähän juuri tämän arduinon yksilöllinen koodi (fiksaa tämä...)
    //mqttClient.setId("ESP_lta_terveiset");
    bool MQTT_yrityslippu=true;
    int MQTT_yritysmaara=0;
    while (!client.connected() && MQTT_yrityslippu) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "IC";
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
    MQTT_yritysmaara=MQTT_yritysmaara+1;
    if (MQTT_yritysmaara>3) {
      MQTT_yrityslippu=false;
      Serial.println("...maksimimäärä lähetysyrityksiä tehty...ohitetaan ja jatketaan...");
    }

  }

  //Simulointia varten...
  //int wifi_ja_mqtt_kunto=1;
  
  return wifi_ja_mqtt_kunto;
}

//Näistä toiminnoista voi olla erityistä hyötyä WLANin virrankulutuksen optimoinnissa...
void disableWiFi(){
    adc_power_off();
    WiFi.disconnect(true);  // Disconnect from the network
    WiFi.mode(WIFI_OFF);    // Switch WiFi off
    //Ilmaistaan keskiledin sammutuksella se, että WLAN on pois päältä...
    digitalWrite(keskiledPIN, LOW);
}

void enableWiFi(){
    //Ilmaistaan keskiledillä se, että WLAN on päällä...
    digitalWrite(keskiledPIN, HIGH);
    adc_power_on();
    WiFi.disconnect(false);  // Reconnect the network
    WiFi.mode(WIFI_STA);    // Switch WiFi off
 
    Serial2.println("KAYNNISTETAAN WLAN...");
    WiFi.begin(ssid, pass);
 
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial2.print(".");
    }
 
    Serial2.println("");
    Serial2.println("WLAN OK!");
    Serial2.println("IP address: ");
    Serial2.println(WiFi.localIP());

    //MQTT-asiat
    client.setServer(broker,port);
}



void loop() {
  //Periaate; aina ensin mitataan, ja kun on mitattu, harkitaan tulisiko tehdä myös tietojen lähetys.
  //
  //Esimerkiksi; mikäli mittauksessa käy niin että jokin menee pieleen, voidaan mitata uudestaan ja kun varmasti on saatu
  //luotettavat tiedot mitattua, vasta sitten lähetetään
  //
  //Lisäksi; tietojen lähettämisen huolellinen harkinta voi auttaa myös virrankulutuksen optimoinnissa
  //
  Serial.println("|");
  Serial.println("");
  Serial.println("#######################################################");
  Serial.println("#          M I T T A U K S E T                        #");
  Serial.println("#######################################################");

  delay(sekuntiviive);


  //Mittausten ajaksi laitetaan vihreä ledi päälle...
  neopixelWrite(RGB_BUILTIN,0,128,0);

  temp_raw=mittaa_temp();
  delay(perusviive);
  humidity_raw=mittaa_humidity();
  delay(perusviive);
  co2_raw=mittaa_co2();
  delay(perusviive);
  moisture_raw=mittaa_moisture();
  delay(perusviive);
  nh3_raw=mittaa_nh3();
  delay(perusviive);

  neopixelWrite(RGB_BUILTIN,0,0,0);

  aikaleima_juuri_nyt=millis();
  aikaero_viimeisimpaan_lahetykseen=aikaleima_juuri_nyt-viimeksi_tehdyn_lahetyksen_aikaleima;
  Serial.println("|");
  Serial.print("Viimeisesta lahetyksesta kulunut aikaa: ");
  Serial.print(aikaero_viimeisimpaan_lahetykseen);

  

  if (aikaero_viimeisimpaan_lahetykseen>tietojen_lahetysvali) {

    enableWiFi();

    //Näillä keveämmillä keinoilla voi myös optimoida virrankulutusta, mutta enable/disable voi olla parempi...
    //Aktivoidaan WLAN:
    //WiFi.setSleep(false); 
    //Nukutetaan WLAN:
    //WiFi.setSleep(true);   

    int onko_yhteys_kunnossa=tarkista_yhteys_ja_nosta_ylos_jos_tarvetta(ssid,pass,broker,port);
    
    if (onko_yhteys_kunnossa==1)
    {
      //Ja lähetystoiminnot:
      //Serial.println("|");
      Serial.println("|");
      Serial.println("");
      Serial.println("#######################################################");
      Serial.println("#          T I E T O J E N    L A H E T Y S            >------>");
      Serial.println("#######################################################");


      //Lähetyksen ajaksi laitetaan ledi päälle...
      neopixelWrite(RGB_BUILTIN,0,0,128);
      
      laheta_tieto(temp_raw,"temp_simu");
      laheta_tieto(humidity_raw,"humidity_simu");
      laheta_tieto(co2_raw,"co2_simu");
      laheta_tieto(moisture_raw,"moisture_simu");
      laheta_tieto(nh3_raw,"nh3_simu");

      //...ja kun lähetetty, ledi sammutetaan!
      neopixelWrite(RGB_BUILTIN,0,0,0);
      
    
    

      //Tallennetaan viimeisin hetki milloin lähetetty...
      viimeksi_tehdyn_lahetyksen_aikaleima = millis();
      disableWiFi();
      delay(viisiviive);

    }
    else
    {
      Serial.println("|OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO|");
      Serial.println("| Yhteysongelmia, tarkistettava yhteydet, tietoja ei lahetetty!  |");
      Serial.println("|OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO|");
      disableWiFi();
    }
    
  
  }

}

