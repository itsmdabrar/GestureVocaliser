#include <Wire.h>
#include <MPU6050.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>

// WiFi Credentials - replace with your own
const char* ssid = "F22";
const char* password = "mdab23092004";

// Web Server setup
WebServer server(80);

// Define pins for flex sensors
const int flexPins[] = {32,35,34,39,36};  // ADC pins on ESP32
const int numSensors = 5;

// Variables for sensor readings
float flexValues[numSensors];
float baselineValues[numSensors];
const float BRIDGE_RESISTANCE = 10000.0;  // 10k bridge resistance

// MPU6050 object
MPU6050 mpu;

// Struct to name parameters more explicitly
struct GestureParameters {
  float index;
  float middle;
  float ring;
  float pinky;
  float thumb;
  float angle;
};

GestureParameters currentGesture;
String lastDetectedGesture = "None";

// Function prototypes to resolve compilation issues
void calibrateSensors();
void handleGesture();
void handleSpeak();
void handleTTS();
void handleRoot();

void setup() {
  Serial.begin(115200);
  
  // Initialize WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());

  // Setup mDNS responder
  if (MDNS.begin("gestureglove")) {
    Serial.println("MDNS responder started");
  }
  
  // Initialize I2C for MPU6050
  Wire.begin();
  
  // Initialize MPU6050
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while(1);
  }
  
  // Initialize ADC
  analogReadResolution(12);  // ESP32 has 12-bit ADC
  
  // Calibration phase
  Serial.println("Calibrating sensors... Keep fingers straight and hand steady");
  calibrateSensors();
  Serial.println("Calibration complete!");

  // Setup Web Server Routes
  server.on("/", handleRoot);
  server.on("/gesture", handleGesture);
  server.on("/speak", handleSpeak);
  server.on("/tts", handleTTS);
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void handleRoot() {
  String html = String("<!DOCTYPE html>") +
    "<html lang=\"en\">" +
    "<head>" +
      "<meta charset=\"UTF-8\">" +
      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">" +
      "<title>Gesture Glove Interface</title>" +
      "<style>" +
        "body { font-family: Arial, sans-serif; background-color: #f0f0f0; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }" +
        ".container { background-color: white; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); padding: 20px; text-align: center; }" +
        ".gesture-display { font-size: 2em; margin: 20px; color: #333; }" +
        "button { background-color: #4CAF50; color: white; border: none; padding: 10px 20px; margin: 10px; border-radius: 5px; cursor: pointer; }" +
      "</style>" +
    "</head>" +
    "<body>" +
      "<div class=\"container\">" +
        "<h1>Gesture Glove</h1>" +
        "<div id=\"gestureDisplay\" class=\"gesture-display\">-</div>" +
        "<button onclick=\"fetchGesture()\">Refresh Gesture</button>" +
        "<button onclick=\"speakGesture()\">Speak Gesture</button>" +
      "</div>" +
      "<script>" +
        "function fetchGesture() {" +
          "fetch('/gesture')" +
            ".then(response => response.text())" +
            ".then(data => {" +
              "document.getElementById('gestureDisplay').textContent = data;" +
            "});" +
        "}" +
        "function speakGesture() {" +
          "fetch('/gesture')" +
            ".then(response => response.text())" +
            ".then(data => {" +
              "if ('speechSynthesis' in window) {" +
                "let utterance = new SpeechSynthesisUtterance(data);" +
                "window.speechSynthesis.speak(utterance);" +
              "}" +
            "});" +
        "}" +
        "setInterval(fetchGesture, 1000);" +
      "</script>" +
    "</body>" +
    "</html>";
  
  server.send(200, "text/html", html);
}

void handleGesture() {
  server.send(200, "text/plain", lastDetectedGesture);
}

void handleSpeak() {
  server.send(200, "text/plain", "Speak request received for: " + lastDetectedGesture);
}

void handleTTS() {
  server.send(200, "text/plain", "TTS placeholder");
}

void loop() {
  // Handle web server clients
  server.handleClient();
  
  // Read and process all sensors
  readFlexSensors();
  readAngleSensor();
  
  // Continuously display current ratios and angle
  displayCurrentReadings();
  
  // Check for specific gestures
  checkGestures();
  
  delay(700);  // Slight delay to prevent overwhelming
}

void calibrateSensors() {
  // Take multiple readings for more accurate calibration
  const int numReadings = 10;
  
  // Calibrate flex sensors
  for(int i = 0; i < numSensors; i++) {
    float sum = 0;
    for(int j = 0; j < numReadings; j++) {
      sum += calculateResistance(analogRead(flexPins[i]));
      delay(100);
    }
    baselineValues[i] = sum / numReadings;
  }
}

void readFlexSensors() {
  for(int i = 0; i < numSensors; i++) {
    // Calculate ratio of current resistance to baseline
    float currentResistance = calculateResistance(analogRead(flexPins[i]));
    float ratio = currentResistance / baselineValues[i];
    
    // Update gesture parameters based on sensor index
    switch(i) {
      case 0: currentGesture.thumb = ratio; break;
      case 1: currentGesture.index = ratio; break;
      case 2: currentGesture.middle = ratio; break;
      case 3: currentGesture.ring = ratio; break;
      case 4: currentGesture.pinky = ratio; break;
    }
  }
}

void readAngleSensor() {
  // Read raw accelerometer data
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  
  // Calculate angle in degrees using arctangent
  // This calculates the angle from the horizontal plane
  float angle = atan2(sqrt(ax*ax + ay*ay), az) * 180.0 / PI;
  
  // Store the absolute angle
  currentGesture.angle = abs(angle);
}

void displayCurrentReadings() {
  Serial.println("Current Readings:");
  Serial.print("Thumb: "); Serial.print(currentGesture.thumb);
  Serial.print(" | Index: "); Serial.print(currentGesture.index);
  Serial.print(" | Middle: "); Serial.print(currentGesture.middle);
  Serial.print(" | Ring: "); Serial.print(currentGesture.ring);
  Serial.print(" | Pinky: "); Serial.print(currentGesture.pinky);
  Serial.print(" | Angle: "); Serial.println(currentGesture.angle);
}

float calculateResistance(int adcValue) {
  float voltage = (adcValue * 3.3) / 4096.0;  // Convert ADC to voltage
  float flexResistance = BRIDGE_RESISTANCE * (3.3 / voltage - 1.0);
  return flexResistance;
}

void checkGestures() {
  // Define gesture thresholds (adjust these values as needed based on your sensor calibration)
  
  // All fingers bent
  bool gesture1 = (currentGesture.index > 1.05 &&
                   currentGesture.middle > 1.05 &&
                   currentGesture.ring > 1.05 &&
                   currentGesture.pinky > 1.05 &&
                   currentGesture.thumb > 1.05 &&
                   currentGesture.angle >= 75 && currentGesture.angle <= 100);
  
  // Gesture A: 4 fingers (except thumb) bent, thumb extended
  bool gesture2 = (currentGesture.index > 1.05 &&
                   currentGesture.middle > 1.05 &&
                   currentGesture.ring > 1.05 &&
                   currentGesture.pinky > 1.05 &&
                   currentGesture.thumb < 1.05 &&
                   currentGesture.angle >= 75 && currentGesture.angle <= 100);
  
  // Gesture B: Index and thumb straight, others bent
  bool gesture3 = (currentGesture.index < 1.05 &&
                   currentGesture.middle > 1.05 &&
                   currentGesture.ring > 1.05 &&
                   currentGesture.pinky > 1.05 &&
                   currentGesture.thumb < 1.05 &&
                   currentGesture.angle >= 75 && currentGesture.angle <= 100);
  
  // Only index and thumb finger bent
  bool gesture4 = (currentGesture.index > 1.05 &&
                   currentGesture.middle < 1.05 &&
                   currentGesture.ring < 1.05 &&
                   currentGesture.pinky < 1.05 &&
                   currentGesture.thumb > 1.05 &&
                   currentGesture.angle >= 75 && currentGesture.angle <= 100);
  
  // Gesture D: Index and middle straight others bent
  bool gesture5 = (currentGesture.index < 1.05 &&
                   currentGesture.middle < 1.05 &&
                   currentGesture.ring > 1.06 &&
                   currentGesture.pinky > 1.05 &&
                   currentGesture.thumb < 1.05 &&
                   currentGesture.angle >= 75 && currentGesture.angle <= 100);

  // middle and ring bent others straight
  bool gesture6 = (currentGesture.index < 1.05 &&
                  currentGesture.middle > 1.05 &&
                  currentGesture.ring > 1.05 &&
                  currentGesture.pinky < 1.05 &&
                  currentGesture.thumb > 1.05 &&
                  currentGesture.angle >= 75 && currentGesture.angle <= 100);

  // all fingers bent except pinky
  bool gesture7 = (currentGesture.index > 1.05 &&
                  currentGesture.middle > 1.05 &&
                  currentGesture.ring > 1.05 &&
                  currentGesture.pinky < 1.03 &&
                  currentGesture.thumb > 1.05 &&
                  currentGesture.angle >= 75 && currentGesture.angle <= 100);

  // all fingers bent except index
  bool gesture8 = (currentGesture.index < 1.05 &&
                  currentGesture.middle > 1.05 &&
                  currentGesture.ring > 1.05 &&
                  currentGesture.pinky > 1.05 &&
                  currentGesture.thumb > 1.05 &&
                  currentGesture.angle >= 75 && currentGesture.angle <= 100);

  // all fingers open
  bool gesture9 = (currentGesture.index < 1.05 &&
                  currentGesture.middle < 1.05 &&
                  currentGesture.ring < 1.05 &&
                  currentGesture.pinky < 1.05 &&
                  currentGesture.thumb < 1.05 &&
                  currentGesture.angle >= 75 && currentGesture.angle <= 100);

  bool gesture10 = (currentGesture.index > 1.05 &&
                   currentGesture.middle > 1.05 &&
                   currentGesture.ring > 1.05 &&
                   currentGesture.pinky > 1.05 &&
                   currentGesture.thumb < 1.05 &&
                   currentGesture.angle >= 130 && currentGesture.angle <= 170);
  // Update last detected gesture (priority order if multiple match)
  if (gesture1) {
    lastDetectedGesture = "Hello, how are you?";
    Serial.println("Detected Gesture: Hello, how are you?");
  } else if (gesture2) {
    lastDetectedGesture = "I need help";
    Serial.println("Detected Gesture: I need help");
  } else if (gesture3) {
    lastDetectedGesture = "Thank you";
    Serial.println("Detected Gesture: Thank you");
  } else if (gesture4) {
    lastDetectedGesture = "Yes, please";
    Serial.println("Detected Gesture: Yes, please");
  } else if (gesture5) {
    lastDetectedGesture = "No";
    Serial.println("Detected Gesture: No");
  }  else if (gesture6) {
    lastDetectedGesture = "Bye";
    Serial.println("Detected Gesture: Bye");
  } else if (gesture7) {
    lastDetectedGesture = "Excuse me";
    Serial.println("Detected Gesture: Excuse me");
  } else if (gesture8) {
    lastDetectedGesture = "Wait a moment";
    Serial.println("Detected Gesture: Wait a moment");
  } else if (gesture9) {
    lastDetectedGesture = "Bless you";
    Serial.println("Detected Gesture: Bless you");
  } else if (gesture10) {
    lastDetectedGesture = "Sure";
    Serial.println("Detected Gesture: Sure");
  } else {
    lastDetectedGesture = "None";
  }
}