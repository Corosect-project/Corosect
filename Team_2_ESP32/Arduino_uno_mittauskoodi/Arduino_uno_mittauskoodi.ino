/* 
Ohjelma laskee arduino unolle suunnitellussa ohjelmassa virran ja jännitteen ennalta määrritellyn vastuksen yli.
Program counts arduino uno pinout program current and voltages over resistor.
*/

double voltage = 0.0, current = 0.0, power = 0.0, resistorvalue = 2.2; //Alkuarvo jännitteelle, virta oletus, luodaan resistanssi ja teho. Starting value for voltage, current, resistor and power 
int voltageport = A0; //Jännitteenlukuportti

void setup() {
  Serial.begin(9600); //Avataan sarjaportti Opening serialport
}

void loop() {
  voltage = (analogRead(voltageport) * 5.0) / 1024.0; //U
  current = voltage / resistorvalue; //I=U/R
  power = voltage * current; //P=U*I
  Serial.println(voltage+" "+current+" "+power); //Jännite, virta ja teho tulostus. Voltage, current and power print.
  delay(1000); //Viive delay