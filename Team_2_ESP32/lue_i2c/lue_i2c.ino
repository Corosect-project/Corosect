#include <Wire.h>
#include "driver/i2c.h"
i2c_rw_t = I2C_MODE_MASTER;


int i2c_master_port = 0;
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = 1,         // select GPIO specific to your project
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = 0,         // select GPIO specific to your project
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = I2C_MASTER_FREQ_HZ,  // select frequency specific to your project
    .clk_flags = 0,                          // you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here
};


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Wire.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  Wire.requestFrom(41, 16);    // request 6 bytes from peripheral device #8


  while (Wire.available()) { // peripheral may send less than requested

    char c = Wire.read(); // receive a byte as character

    Serial.print(c);         // print the character

  }


}
