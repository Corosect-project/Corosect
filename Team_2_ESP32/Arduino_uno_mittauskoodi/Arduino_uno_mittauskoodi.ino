/* 
Ohjelma laskee arduino unolle suunnitellussa ohjelmassa virran ja jännitteen ennalta määrritellyn vastuksen yli.
Program counts arduino uno pinout program current and voltages over resistor.
*/

double jannite = 0.0, virta = 0.0, teho = 0.0, vastuksenarvo = 2.2; //Alku arvo jännitteelle ja virta oletus ja luodaan resistanssi ja teho
int janniteportti = A1, virtaportti = A0;

void setup() {
  Serial.begin(9600);
}

void loop() {
  jannite = (analogRead(virtaportti) * 5.0) / 1024.0; //U
  virta = ((analogRead(janniteportti) * 5.0) / 1024.0) / (vastuksenarvo); //I=U/R
  teho = jannite*virta; //P=UI
  Serial.print(virta); //Virta
  Serial.print(" ");
  Serial.print(jannite); //Jännite
  Serial.print(" ");
  Serial.println(teho);  
  delay(1000);