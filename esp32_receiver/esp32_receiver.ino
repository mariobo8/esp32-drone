#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <HardwareSerial.h>
#include "sbus.h"

#define DEBUG 1
#define SBUS_MIN 172
#define SBUS_MAX 1811
#define SBUS_MID ((SBUS_MAX + SBUS_MIN) / 2)
#define FAILSAFE_TIMEOUT 3000  // milliseconds
#define PLOT_WIDTH 50  // Width of the ASCII plot
#define PLOT_INTERVAL 100  // Plot update interval in milliseconds
#define LED_PIN 2  // Built-in LED on most ESP32 boards
#define LED_BLINK_DURATION 100  // LED blink duration in milliseconds

// Initialize the SBUS transmission object
bfs::SbusTx sbusTx(&Serial1, 10, 9, true);
// Create data for 16 channels
bfs::SbusData data;
bfs::SbusData previousData;  // To store the previous data for comparison

// Flag to track if initial command has been sent
bool initialCommandSent = false;

unsigned long lastSbusSend = micros();
unsigned long lastDataReceived = 0;
unsigned long lastPlotTime = 0;
unsigned long ledTurnOffTime = 20;  // Time to turn off the LED
const float sbusFrequency = 50.0;

char Commands[1024];

// Flag to track if connection is established
bool connectionEstablished = false;
bool dataChanged = false;  // Flag to track if data has changed

// MAC Address of the sender ESP32 
uint8_t senderAddress[] = { 0x88, 0x13, 0xBF, 0x03, 0x0A, 0x8C };

// New variables for telemetry
#define TELEMETRY_BUFFER_SIZE 256
char telemetryBuffer[TELEMETRY_BUFFER_SIZE];
unsigned long lastTelemetrySend = 0;
#define TELEMETRY_INTERVAL 20  // Send telemetry every 20ms

// Function declarations
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void plotData();
void sendTelemetry();

void setup() {
  Serial.begin(115200);
  Serial.println("Starting setup...");  // Debug print
  delay(1000);

  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // Ensure LED is off at start

  Serial.println("Setting WiFi mode...");  // Debug print
  WiFi.mode(WIFI_STA);

  Serial.println("Initializing ESP-NOW...");  // Debug print
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  Serial.println("ESP-NOW initialized successfully");  // Debug print

  // Register callbacks
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  Serial.println("Callbacks registered");  // Debug print

  // Initialize SBUS
  sbusTx.Begin();
  Serial.println("SBUS initialized");

  data.failsafe = false;
  data.ch17 = true;
  data.ch18 = true;
  data.lost_frame = false;

  // Initialize previousData
  for (int i = 0; i < 16; i++) {
    previousData.ch[i] = SBUS_MID;
  }
  previousData.failsafe = false;
  previousData.ch17 = true;
  previousData.ch18 = true;
  previousData.lost_frame = false;

  lastSbusSend = micros();
  lastDataReceived = millis();
  lastPlotTime = millis();
  lastTelemetrySend = millis();

  Serial.println("ESP32 SBUS Controller with Serial Plotting and LED Indicator");
  Serial.println("Waiting for connection to be established...");
  Serial.println("------------------------------------------");
  Serial.println("Setup completed");  // Debug print
}

// Callback function that will be executed when data is received
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
  memcpy(&Commands, incomingData, sizeof(Commands));
  
  int armed = 0, throttle = 0, pitch = 0, roll = 0, yaw = 0;
  int um_armed = 0, um_throttle = 0, um_pitch = 0, um_roll = 0, um_yaw = 0;
  
  if (sscanf(Commands, "A:%d,T:%d,P:%d,R:%d,Y:%d", &um_armed, &um_throttle, &um_pitch, &um_roll, &um_yaw) == 5) {
    armed = constrain(map(um_armed, 0, 1, SBUS_MIN, SBUS_MAX), SBUS_MIN, SBUS_MAX);
    throttle = constrain(map(um_throttle, 0, 100, SBUS_MIN, SBUS_MAX), SBUS_MIN, SBUS_MAX);
    yaw = constrain(map(um_yaw, -100, 100, SBUS_MIN, SBUS_MAX), SBUS_MIN, SBUS_MAX);
    pitch = constrain(map(um_pitch, -100, 100, SBUS_MIN, SBUS_MAX), SBUS_MIN, SBUS_MAX);
    roll = constrain(map(um_roll, -100, 100, SBUS_MIN, SBUS_MAX), SBUS_MIN, SBUS_MAX);

    // Check if data has changed
    dataChanged = (data.ch[4] != armed) || (data.ch[0] != throttle) || (data.ch[1] != yaw) || 
                  (data.ch[2] != pitch) || (data.ch[3] != roll);

    // Update SBUS data
    data.ch[4] = armed;     // Armed
    data.ch[0] = throttle;  // Throttle
    data.ch[1] = roll;      // Yaw
    data.ch[2] = pitch;     // Pitch
    data.ch[3] = yaw;       // Roll

    lastDataReceived = millis();
  }
}

// Callback function for when data is sent (telemetry)
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optional: Handle send status
  if (status == ESP_NOW_SEND_SUCCESS) {
    // Serial.println("Telemetry sent successfully");
  } else {
    // Serial.println("Error sending telemetry");
  }
}

// Function to plot data on serial monitor
void plotData() {
  Serial.println("\n--- Channel Values ---");
  const char* channelNames[] = {"Throttle", "Yaw", "Pitch", "Roll", "Armed"};
  for (int i = 0; i < 5; i++) {
    int value = data.ch[i];
    int plotPosition = map(value, SBUS_MIN, SBUS_MAX, 0, PLOT_WIDTH - 1);
    Serial.printf("%-8s |", channelNames[i]);
    for (int j = 0; j < PLOT_WIDTH; j++) {
      if (j == plotPosition) {
        Serial.print("O");
      } else {
        Serial.print("-");
      }
    }
    Serial.printf("| %4d\n", value);
  }
  Serial.println("------------------------------------------");
}

void sendTelemetry() {
  // Read telemetry data from Serial1 (same UART as SBUS)
  if (Serial1.available()) {
    int bytesRead = Serial1.readBytes(telemetryBuffer, TELEMETRY_BUFFER_SIZE - 1);
    telemetryBuffer[bytesRead] = '\0';  // Null-terminate the string

    // Send telemetry data via ESP-NOW
    esp_err_t result = esp_now_send(senderAddress, (uint8_t *)telemetryBuffer, bytesRead);
    
    if (result == ESP_OK) {
      // Optional: Indicate successful send
      digitalWrite(LED_PIN, HIGH);
      ledTurnOffTime = millis() + LED_BLINK_DURATION;
    }
  }
}

void loop() {
  unsigned long currentTime = millis();

  // Check for failsafe condition
  if (currentTime - lastDataReceived > FAILSAFE_TIMEOUT) {
    if (!data.failsafe) {
      Serial.println("No data received - Entering failsafe mode");
      data.failsafe = true;
      // Set channels to failsafe values
      data.ch[0] = SBUS_MIN;
      for (int i = 1; i < 16; i++) {
        data.ch[i] = SBUS_MID;
      }
      dataChanged = true;  // Ensure we plot when entering failsafe
    }
  } else {
    if (data.failsafe) {
      Serial.println("Data received - Exiting failsafe mode");
      data.failsafe = false;
      dataChanged = true;  // Ensure we plot when exiting failsafe
    }
  }

  // Send SBUS data at the specified frequency
  if (micros() - lastSbusSend > 1e6 / sbusFrequency) {
    lastSbusSend = micros();
    sbusTx.data(data);
    sbusTx.Write();
  }

  // Send telemetry at specified interval
  if (currentTime - lastTelemetrySend > TELEMETRY_INTERVAL) {
    sendTelemetry();
    lastTelemetrySend = currentTime;
  }

  // Update plot at specified interval and only if data has changed
  if (currentTime - lastPlotTime > PLOT_INTERVAL && dataChanged) {
    plotData();
    lastPlotTime = currentTime;
    dataChanged = false;  // Reset the flag after plotting
    // Blink LED when data is received
    digitalWrite(LED_PIN, HIGH);
    ledTurnOffTime = millis() + LED_BLINK_DURATION;
  }

  // Turn off LED after blink duration
  if (currentTime >= ledTurnOffTime && digitalRead(LED_PIN) == HIGH) {
    digitalWrite(LED_PIN, LOW);
  }
}