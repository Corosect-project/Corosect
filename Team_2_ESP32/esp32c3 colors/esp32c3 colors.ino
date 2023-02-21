void setup() {}

void loop() {  
  neopixelWrite(RGB_BUILTIN,255,0,0); //Punainen Red
  delay(1000);
  neopixelWrite(RGB_BUILTIN,0,255,0); //Vihreä Green
  delay(1000);
  neopixelWrite(RGB_BUILTIN,0,0,255); //Sininen Blue
  delay(1000);
  neopixelWrite(RGB_BUILTIN,255,255,0); //Keltainen Yellow
  delay(1000);
  neopixelWrite(RGB_BUILTIN,255,255,255); //Valkoinen White
  delay(1000);
  neopixelWrite(RGB_BUILTIN,255,0,255); //Magnetta
  delay(1000);
  neopixelWrite(RGB_BUILTIN,0,0,0); //Musta pois päältä Black lights out
  delay(1000);
}
