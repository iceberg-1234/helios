#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

// --- Pin Definitions ---
// Motor Driver (L298N / TB6612FNG)
#define PIN_IN1 25
#define PIN_IN2 26
#define PIN_ENA 27
#define PIN_IN3 32
#define PIN_IN4 33
#define PIN_ENB 14

// HC-SR04 Ultrasonic Sensor
#define PIN_TRIG 5
#define PIN_ECHO 17

// Battery ADC (Voltage Divider: 100k / 20k, factor ~6.0)
#define PIN_BATTERY 34

// I2C Buses for 3x BH1750 (Wire: SDA=21, SCL=22 / Wire1: SDA=18, SCL=19)
#define I2C_ADDR_BH1750_LOW  0x23
#define I2C_ADDR_BH1750_HIGH 0x5C

WebServer server(80);

enum Mode { MODE_STOP = 0, MODE_SUN = 1, MODE_SHADE = 2 };
Mode currentMode = MODE_SUN;

float luxLeft = 0, luxCenter = 0, luxRight = 0;
float distanceCm = 100.0;
float batteryVoltage = 12.0;

unsigned long lastMoveTime = 0;
bool isMoving = false;

// Motor Control Helper
void setMotorSpeed(int leftPwm, int rightPwm) {
  // Left Motor
  if (leftPwm > 0) {
    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, LOW);
  } else if (leftPwm < 0) {
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, HIGH);
    leftPwm = -leftPwm;
  } else {
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
  }
  ledcWrite(0, constrain(leftPwm, 0, 255));

  // Right Motor
  if (rightPwm > 0) {
    digitalWrite(PIN_IN3, HIGH);
    digitalWrite(PIN_IN4, LOW);
  } else if (rightPwm < 0) {
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, HIGH);
    rightPwm = -rightPwm;
  } else {
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, LOW);
  }
  ledcWrite(1, constrain(rightPwm, 0, 255));
}

void stopMotors() {
  setMotorSpeed(0, 0);
}

// Read raw BH1750 without external library
float readBH1750(TwoWire &bus, uint8_t addr) {
  bus.beginTransmission(addr);
  bus.write(0x10); // Continuously H-Resolution Mode
  bus.endTransmission();
  delay(180);

  if (bus.requestFrom(addr, (uint8_t)2) == 2) {
    uint16_t val = (bus.read() << 8) | bus.read();
    return val / 1.2f;
  }
  return 0.0f;
}

// HC-SR04 Distance Measurement
float readDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duration = pulseIn(PIN_ECHO, HIGH, 30000); // 30ms timeout (~5m)
  if (duration == 0) return 999.0;
  return duration * 0.0343f / 2.0f;
}

// Battery Voltage Read
float readBattery() {
  int raw = analogRead(PIN_BATTERY);
  // ADC range 0-4095 (3.3V ref), voltage divider 100k/20k = 6.0 multiplier
  return (raw / 4095.0f) * 3.3f * 6.0f;
}

// Web Server Handlers
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Helios Planter</title><style>";
  html += "body{font-family:sans-serif;text-align:center;padding:20px;background:#f0f4f8;}";
  html += ".btn{display:inline-block;padding:12px 24px;margin:8px;font-size:18px;border:none;border-radius:8px;color:#fff;cursor:pointer;}";
  html += ".btn-sun{background:#ff9800;}.btn-shade{background:#3f51b5;}.btn-stop{background:#e91e63;}";
  html += ".card{background:#fff;padding:16px;border-radius:12px;box-shadow:0 2px 8px rgba(0,0,0,0.1);margin:16px auto;max-width:360px;}";
  html += "</style></head><body>";
  html += "<h2>Sunflower Planter (Helios)</h2>";
  html += "<div class='card'>";
  html += "<p>Mode: <b id='mode'>--</b></p>";
  html += "<p>Battery: <b id='bat'>--</b> V</p>";
  html += "<p>Distance: <b id='dist'>--</b> cm</p>";
  html += "<p>Lux [L / C / R]: <b id='lux'>--</b></p>";
  html += "</div>";
  html += "<div>";
  html += "<button class='btn btn-sun' onclick=\"fetch('/setMode?m=1')\">햇빛 추적</button><br>";
  html += "<button class='btn btn-shade' onclick=\"fetch('/setMode?m=2')\">햇빛 회피</button><br>";
  html += "<button class='btn btn-stop' onclick=\"fetch('/setMode?m=0')\">정지</button>";
  html += "</div>";
  html += "<script>";
  html += "setInterval(()=>{fetch('/status').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('mode').innerText = d.mode==1?'SUN':d.mode==2?'SHADE':'STOP';";
  html += "document.getElementById('bat').innerText = d.bat.toFixed(1);";
  html += "document.getElementById('dist').innerText = d.dist.toFixed(1);";
  html += "document.getElementById('lux').innerText = d.luxL.toFixed(0)+' / '+d.luxC.toFixed(0)+' / '+d.luxR.toFixed(0);";
  html += "});},1000);";
  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{";
  json += "\"mode\":" + String(currentMode) + ",";
  json += "\"bat\":" + String(batteryVoltage, 2) + ",";
  json += "\"dist\":" + String(distanceCm, 1) + ",";
  json += "\"luxL\":" + String(luxLeft, 1) + ",";
  json += "\"luxC\":" + String(luxCenter, 1) + ",";
  json += "\"luxR\":" + String(luxRight, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetMode() {
  if (server.hasArg("m")) {
    currentMode = (Mode)server.arg("m").toInt();
    stopMotors();
  }
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);

  // Motor Pins
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  ledcSetup(0, 1000, 8); // Channel 0, 1kHz, 8-bit
  ledcAttachPin(PIN_ENA, 0);
  ledcSetup(1, 1000, 8); // Channel 1, 1kHz, 8-bit
  ledcAttachPin(PIN_ENB, 1);
  stopMotors();

  // Sensors
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BATTERY, INPUT);

  // Dual I2C for 3x BH1750
  Wire.begin(21, 22);   // Bus 0 (Left: 0x23, Right: 0x5C)
  Wire1.begin(18, 19);  // Bus 1 (Center: 0x23)

  // Wi-Fi AP Mode
  WiFi.softAP("Helios-Planter", "12345678");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/setMode", handleSetMode);
  server.begin();
}

void loop() {
  server.handleClient();

  // Read sensors every 500ms
  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead > 500) {
    lastSensorRead = millis();
    luxLeft = readBH1750(Wire, I2C_ADDR_BH1750_LOW);
    luxRight = readBH1750(Wire, I2C_ADDR_BH1750_HIGH);
    luxCenter = readBH1750(Wire1, I2C_ADDR_BH1750_LOW);
    distanceCm = readDistance();
    batteryVoltage = readBattery();
  }

  // Safety: Obstacle detected within 20cm
  if (distanceCm < 20.0f) {
    // Avoid obstacle: reverse slightly and turn right
    setMotorSpeed(-120, -120);
    delay(400);
    setMotorSpeed(140, -140);
    delay(500);
    stopMotors();
    return;
  }

  // Intermittent movement: run 1.5s, pause 3s to save battery and stabilize plants
  unsigned long now = millis();
  if (currentMode == MODE_STOP) {
    stopMotors();
    return;
  }

  // Decision logic
  int baseSpeed = 150; // PWM (0-255)
  float threshold = 50.0f; // lux difference threshold

  if (currentMode == MODE_SUN) {
    // Follow brightest direction
    if (luxLeft > luxCenter + threshold && luxLeft > luxRight) {
      setMotorSpeed(100, 180); // Turn left
    } else if (luxRight > luxCenter + threshold && luxRight > luxLeft) {
      setMotorSpeed(180, 100); // Turn right
    } else {
      setMotorSpeed(baseSpeed, baseSpeed); // Go forward towards light
    }
  } else if (currentMode == MODE_SHADE) {
    // Follow darkest direction (avoid brightest)
    if (luxLeft < luxCenter - threshold && luxLeft < luxRight) {
      setMotorSpeed(100, 180); // Turn towards left (darker)
    } else if (luxRight < luxCenter - threshold && luxRight < luxLeft) {
      setMotorSpeed(180, 100); // Turn towards right (darker)
    } else {
      setMotorSpeed(baseSpeed, baseSpeed); // Move towards center shade
    }
  }
}
