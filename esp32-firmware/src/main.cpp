#include <Arduino.h>

#include "config.h"
#include "console.h"
#include "controller.h"
#include "server_link.h"
#include "outputs.h"
#include "sensors.h"

Outputs outputs;
Sensors sensors;
Controller controller(sensors, outputs);
Console console(controller, sensors, outputs);
ServerLink serverLink(controller, sensors, outputs);

void setup() {
  // Đưa đầu ra về trạng thái an toàn trước mọi việc khác.
  outputs.begin();

  Serial.begin(115200);
  delay(200);

  const uint32_t now = millis();
  sensors.begin(now);
  controller.begin(now);
  console.begin();
  serverLink.begin();
}

void loop() {
  const uint32_t now = millis();
  sensors.update(now);
  controller.update(now);
  outputs.update(now);
  console.update(now);
  serverLink.update(now);
}
