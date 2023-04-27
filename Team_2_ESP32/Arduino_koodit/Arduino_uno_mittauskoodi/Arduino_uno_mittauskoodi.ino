/* 
Ohjelma laskee arduino unolle suunnitellussa ohjelmassa virran ja jännitteen ennalta määrritellyn vastuksen yli.
Vastuksen arvo on 2,2 Ohm.
Program counts arduino uno pinout program current and voltages over resistor.
Resistor value is 2.2 Ohm.
*/

double voltage = 0.0, current = 0.0, power = 0.0, resistorvalue = 2.2; //Alkuarvo jännitteelle, virta oletus, luodaan resistanssi ja teho. Starting value for voltage, current, resistor and power 
int voltageport = A0; //Jännitteen lukuportti current reading port

void setup() {
  Serial.begin(9600); //Avataan sarjaportti Opening serialport
}

void loop() {
  voltage = (analogRead(voltageport) * 5.0) / 1024.0; //U=(Uin*5V)/1024 5 Volttia on maksimimäärä. 1024 on 10 bittiä. 5 volts is max voltage. 1024 is 10 bits.
  current = voltage / resistorvalue; //I=U/R
  power = voltage * current; //P=U*I
  Serial.println(String(voltage) + " " + String(current) + " " + String(power)); //Jännite, virta ja teho tulostus. Voltage, current and power print.
  delay(1000); //Viive delay
