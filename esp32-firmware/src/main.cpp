#include <Arduino.h>

#include "config.h"
#include "console.h"
#include "controller.h"
#include "door.h"
#include "server_link.h"
#include "outputs.h"
#include "sensors.h"

Outputs outputs;
Sensors sensors;
Door door;
Controller controller(sensors, outputs, door);
Console console(controller, sensors, outputs, door);
ServerLink serverLink(controller, sensors, outputs, door);

void setup() {
  // Đưa đầu ra về trạng thái an toàn trước mọi việc khác.
  outputs.begin();

  Serial.begin(115200);
  delay(200);

  const uint32_t now = millis();
  door.begin(now);
  sensors.begin(now);
  controller.begin(now);
  console.begin();
  serverLink.begin();
}

void loop() {
  const uint32_t now = millis();
  sensors.update(now);
  controller.update(now);
  door.update(now);
  outputs.update(now);
  console.update(now);
  serverLink.update(now);
}
