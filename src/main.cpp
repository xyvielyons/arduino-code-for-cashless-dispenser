#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
void callback(char *topic, byte *payload, unsigned int length);
const char* ssid = "xyvie";
const char* password = "xyvielyonsmbugua";

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *topic = "esp32/pumpstatus";
const char *topic2 = "esp32/test1";
const char *mqtt_username = "xyvielyons";
const char *mqtt_password = "XyvieLyons7@gmail.com";
const char *healthTopic = "esp32/health";  // Health check topic

#define flowMeterPin 33  // Flow meter signal wire connected to pin 2
#define PULSES_PER_LITER 450  // Pulses per liter (typical for YF-S201)



volatile int pulseCount = 0;  // Counter for the number of pulses
float flowRate = 0.0;         // Flow rate in liters per minute
float totalLiters = 0.0;      // Total volume of water dispensed
float targetLitres = 0.0;
bool dispensing = false;



WiFiClient espClient;
PubSubClient client(espClient);

// Interrupt function to count pulses from the flow meter
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}
void setup() {
  // Set software serial baud to 115200;
 Serial.begin(9600);
 pinMode(flowMeterPin, INPUT_PULLUP);  // Set flow meter pin as input
 attachInterrupt(digitalPinToInterrupt(flowMeterPin), pulseCounter, FALLING);  // Attach interrupt to count pulses
 pinMode(2,OUTPUT);


 // connecting to a WiFi network
 WiFi.begin(ssid, password);
 while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.println("Connecting to WiFi..");
 }
 Serial.println("Connected to the WiFi network");


 client.setServer(mqtt_server,mqtt_port);
 client.setCallback(callback);

 while(!client.connected()){
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if(client.connect(client_id.c_str(),mqtt_username,mqtt_password)){
          Serial.println("Public emqx mqtt broker connected");
    }else{
          Serial.print("failed with state ");
          Serial.print(client.state());
          delay(2000);
    }
 }

  client.subscribe(topic);
  client.publish(topic2, "Hi I'm ESP32 at your service");


;}

void loop() {
  client.loop();// put your main code here, to run repeatedly:
  // Every second, calculate the flow rate and total volume dispensed
  static unsigned long lastTime = 0;
  unsigned long currentTime = millis();

  // Check if a second has passed
  if (dispensing && currentTime - lastTime >= 100) {
    lastTime = currentTime;

    // Calculate flow rate (L/min) based on pulse count
    flowRate = (pulseCount / float(PULSES_PER_LITER)) * 60.0;

    // Convert flow rate to liters per second and add to the total liters dispensed
    totalLiters += flowRate / 60.0;

    // Print the flow rate and total volume dispensed
    Serial.print("Flow Rate: ");
    Serial.print(flowRate);
    Serial.print(" L/min, Total Liters: ");
    Serial.println(totalLiters);
    // Convert totalLiters to string
    String totalLitersStr = String(totalLiters, 2);  // 2 decimal places for precision

    // Convert flowRate to string
    String flowRateStr = String(flowRate, 2);

    // Create a message to send via MQTT
    String message = "Flow Rate: " + flowRateStr + " L/min, Total Liters: " + totalLitersStr + " TargetLitres: " + targetLitres;

    // Print to Serial for debugging
    Serial.println(message);

    // Publish the message to the MQTT broker
    client.publish(topic2, message.c_str()); // Pass the message as a C-string
    
    if(totalLiters >= targetLitres){
      digitalWrite(2,LOW);
      dispensing = false;
      pulseCount = 0;
      Serial.println("Target volume reached, pump off.");
    }
    // Reset pulse count for the next second
    pulseCount = 0;
  }

  //healthcheckevery 10 seconds
    static unsigned long healthCheckTime = 0;
    if (currentTime - healthCheckTime >= 10000) {
    healthCheckTime = currentTime;
    client.publish(healthTopic, "ESP32 is healthy");
    Serial.println("Health check message sent");
  }


}

void callback(char *topic, byte *payload, unsigned int length){
  Serial.print("message arrived in topic: ");
  Serial.println(topic);
  Serial.print("message:");
  String messageTemp;

  for(int i=0;i<length;i++){
    Serial.print((char) payload[i]);
    messageTemp += (char) payload[i];
  }
  Serial.println();
  Serial.println("-----------------------");
  Serial.println(messageTemp);

  if(messageTemp == "pumpon"){
    digitalWrite(2,HIGH);
    Serial.print("pump on");
  }
  if(messageTemp == "pumpoff"){
    digitalWrite(2,LOW);
    Serial.print("pump off");
  }

  // Parse the MQTT message
  if (messageTemp.startsWith("dispense:")) {
    String litersToDispense = messageTemp.substring(9);  // Get the liters to dispense from the message
    targetLitres = litersToDispense.toFloat();
    // Start dispensing
    pulseCount = 0;           // Reset the pulse count
    totalLiters = 0.0;        // Reset the total liters counter
    digitalWrite(2, HIGH);  // Turn on the pump
    dispensing = true;         // Set the dispensing flag
    Serial.print("Dispensing ");
    Serial.print(targetLitres);
    Serial.println(" liters...");
  }

}



