#include <Arduino_HS300x.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>

float baseHumidity, baseTemp, baseMag;
int baseClear;

// tuned by comparing calm baseline noise vs the response while triggering each event
float humidJumpThreshold = 3.0;
float tempRiseThreshold = 1.0;
float magShiftThreshold = 15.0;
int lightChangeThreshold = 40;

unsigned long cooldownMs = 3000;
unsigned long lastEventTime = 0;
String lastEventLabel = "BASELINE_NORMAL";

float magMagnitude(float x, float y, float z) {
  return sqrt(x * x + y * y + z * z);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!HS300x.begin()) {
    Serial.println("failed to initialize humidity/temperature sensor.");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("failed to initialize imu.");
    while (1);
  }

  if (!APDS.begin()) {
    Serial.println("failed to initialize apds9960 sensor.");
    while (1);
  }

  Serial.println("calibrating baseline, keep the board still...");

  // just average a handful of readings to get a baseline for this room
  float hSum = 0, tSum = 0, mSum = 0;
  long cSum = 0;
  int n = 10;
  for (int i = 0; i < n; i++) {
    hSum += HS300x.readHumidity();
    tSum += HS300x.readTemperature();

    if (IMU.magneticFieldAvailable()) {
      float mx, my, mz;
      IMU.readMagneticField(mx, my, mz);
      mSum += magMagnitude(mx, my, mz);
    }

    int r, g, b, c;
    unsigned long t0 = millis();
    while (!APDS.colorAvailable() && millis() - t0 < 200);
    APDS.readColor(r, g, b, c);
    cSum += c;

    delay(200);
  }
  baseHumidity = hSum / n;
  baseTemp = tSum / n;
  baseMag = mSum / n;
  baseClear = cSum / n;

  Serial.println("event monitor started");
}

void loop() {
  float rh = HS300x.readHumidity();
  float temp = HS300x.readTemperature();

  float mag = 0;
  if (IMU.magneticFieldAvailable()) {
    float mx, my, mz;
    IMU.readMagneticField(mx, my, mz);
    mag = magMagnitude(mx, my, mz);
  }

  int r, g, b, c;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
  }

  int humidJump = (abs(rh - baseHumidity) > humidJumpThreshold) ? 1 : 0;
  int tempRise = ((temp - baseTemp) > tempRiseThreshold) ? 1 : 0;
  int magShift = (abs(mag - baseMag) > magShiftThreshold) ? 1 : 0;
  int lightOrColorChange = (abs(c - baseClear) > lightChangeThreshold) ? 1 : 0;

  String label = "BASELINE_NORMAL";
  if (humidJump || tempRise) {
    label = "BREATH_OR_WARM_AIR_EVENT";
  } else if (magShift) {
    label = "MAGNETIC_DISTURBANCE_EVENT";
  } else if (lightOrColorChange) {
    label = "LIGHT_OR_COLOR_CHANGE_EVENT";
  }
  // don't spam the same event every single loop while the condition is still true
  if (label != "BASELINE_NORMAL") {
    if (label == lastEventLabel && (millis() - lastEventTime) < cooldownMs) {
      label = "BASELINE_NORMAL";
    } else {
      lastEventLabel = label;
      lastEventTime = millis();
    }
  }

  Serial.print("raw,rh=");
  Serial.print(rh, 2);
  Serial.print(",temp=");
  Serial.print(temp, 2);
  Serial.print(",mag=");
  Serial.print(mag, 2);
  Serial.print(",r=");
  Serial.print(r);
  Serial.print(",g=");
  Serial.print(g);
  Serial.print(",b=");
  Serial.print(b);
  Serial.print(",clear=");
  Serial.println(c);

  Serial.print("flags,humid_jump=");
  Serial.print(humidJump);
  Serial.print(",temp_rise=");
  Serial.print(tempRise);
  Serial.print(",mag_shift=");
  Serial.print(magShift);
  Serial.print(",light_or_color_change=");
  Serial.println(lightOrColorChange);

  Serial.print("event,"); Serial.println(label);

  delay(300);
}
