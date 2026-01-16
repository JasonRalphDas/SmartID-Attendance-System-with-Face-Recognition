#include <SPI.h>

void setup() {
  Serial.begin(115200);
  SPI.begin();
  Serial.println("SPI is working!");
}

void loop() {
  delay(1000);
}
