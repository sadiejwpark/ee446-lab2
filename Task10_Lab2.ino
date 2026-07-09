#include <PDM.h>
#include <Arduino_APDS9960.h>
#include <Arduino_BMI270_BMM150.h>

short sampleBuffer[256];
volatile int samplesRead = 0;

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

// these were tuned by watching the raw values print out under
// different conditions and picking a number in between the two clusters
int soundThreshold = 300;
int darkThreshold = 50;
float motionThreshold = 0.05;
int nearThreshold = 80;

float lastAx = 0, lastAy = 0, lastAz = 1.0;

void setup() {
  Serial.begin(115200);
  delay(1500);

  PDM.onReceive(onPDMdata);

  if (!PDM.begin(1, 16000)) {
    Serial.println("failed to start pdm microphone.");
    while (1);
  }

  if (!APDS.begin()) {
    Serial.println("failed to initialize apds9960 sensor.");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("failed to initialize imu.");
    while (1);
  }

  Serial.println("smart workspace classifier started");
}

void loop() {
  int micLevel = 0;
  if (samplesRead) {
    long sum = 0;
    for (int i = 0; i < samplesRead; i++) sum += abs(sampleBuffer[i]);
    micLevel = sum / samplesRead;
    samplesRead = 0;
  }

  int r, g, b, c;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
  }

  // distance between this reading and the last one, so it's basically zero when still
  float motion = 0;
  if (IMU.accelerationAvailable()) {
    float ax, ay, az;
    IMU.readAcceleration(ax, ay, az);
    motion = sqrt(sq(ax - lastAx) + sq(ay - lastAy) + sq(az - lastAz));
    lastAx = ax; lastAy = ay; lastAz = az;
  }

  int prox = 0;
  if (APDS.proximityAvailable()) {
    prox = APDS.readProximity(); // 0 = far, 255 = close
  }

  int sound = (micLevel > soundThreshold) ? 1 : 0;
  int dark = (c < darkThreshold) ? 1 : 0;
  int moving = (motion > motionThreshold) ? 1 : 0;
  int near = (prox > nearThreshold) ? 1 : 0;

  String label;
  if (!sound && !dark && !moving && !near) {
    label = "QUIET_BRIGHT_STEADY_FAR";
  } else if (sound && !dark && !moving && !near) {
    label = "NOISY_BRIGHT_STEADY_FAR";
  } else if (!sound && dark && !moving && near) {
    label = "QUIET_DARK_STEADY_NEAR";
  } else if (sound && !dark && moving && near) {
    label = "NOISY_BRIGHT_MOVING_NEAR";
  } else {
    // anything that doesn't match one of the four exactly, just pick the closest one
    if (moving && near) {
      label = "NOISY_BRIGHT_MOVING_NEAR";
    } else if (dark && near) {
      label = "QUIET_DARK_STEADY_NEAR";
    } else if (sound) {
      label = "NOISY_BRIGHT_STEADY_FAR";
    } else {
      label = "QUIET_BRIGHT_STEADY_FAR";
    }
  }

  Serial.print("raw,mic=");
  Serial.print(micLevel);
  Serial.print(",clear=");
  Serial.print(c);
  Serial.print(",motion=");
  Serial.print(motion, 4);
  Serial.print(",prox=");
  Serial.println(prox);

  Serial.print("flags,sound=");
  Serial.print(sound);
  Serial.print(",dark=");
  Serial.print(dark);
  Serial.print(",moving=");
  Serial.print(moving);
  Serial.print(",near=");
  Serial.println(near);

  Serial.print("state,");
  Serial.println(label);

  delay(200);
}
