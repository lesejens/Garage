/*
  Basic MQTT example

*/

#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <PubSubClient.h>

//#define DEBUG

// DS18S20 Temperature chip I/O
OneWire ds(9);  // on pin 9
byte addr[8];   // Communication Area for 1Wire
int temp;       // Int to store the temperature
char str_temp[4]; // temperature will be transmitted as string


// Update these with values suitable for your network.
byte mac[]    = {  0x90, 0xA2, 0xDA, 0x00, 0x77, 0xE1 };
IPAddress ip(192, 168, 178, 19);
IPAddress gateway(192, 168, 178, 1);                   // internet access via router
IPAddress subnet(255, 255, 255, 0);                   //subnet mask

//IPAddress server(192, 168, 178, 21);
const char* server = "fhem-prod";
char inputTopic[] = "garage/input2/set";

struct InpObject {
  int i_new;
  int i_old;
  int i_reading;
  int i_input;
  long i_debounce;
};

struct InpObject i_Table[8] = {
  {0, 0, 0, 18, 0},
  {0, 0, 0, 3, 0},
  {0, 0, 0, 19, 0},
  {0, 0, 0, 2, 0},
  {0, 0, 0, 8, 0}, // Special Value for Analog in
  {0, 0, 0, 0, 0},
  {0, 0, 0, 8, 0}, // Special Value for Analog in
  {0, 0, 0, 1, 0},
};

struct OutObject {
  int o_output;   // Output Pin of Hardware
  long o_pulse;   // Variable to store time of pulse
};

struct OutObject o_Table[8] = {
  {  7, 0},
  {  6, 0},
  {  5, 0},
  {  4, 0},
  { 15, 0},
  { 14, 0},
  { 16, 0},
  { 17, 0},
};

long debounceDelay = 100;    // the debounce time; increase if the output flickers

// timer variables
unsigned long previousMillis = 0, currentMillis;
const long interval = 300000;           // interval at which to read temperature from 1Wire ensor 300.000 = 5 Minutes


// Ausgabefunktion
void callback(char* topic, byte* payload, unsigned int length) {
#ifdef DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
#endif
  for (int i = 0; i < length; i++) {
#ifdef DEBUG
    Serial.print((char)payload[i]);
#endif
  }
#ifdef DEBUG
  Serial.println();
  Serial.print("Ausgang ");
  Serial.print((char)payload[2]);
  Serial.print(": ");
#endif
  switch (payload[1]) {
    case 'n':
#ifdef DEBUG
      Serial.print("Ein ");
#endif
      digitalWrite(o_Table[payload[2] - 0x30].o_output, HIGH);
      break;
    case 'f':
#ifdef DEBUG
      Serial.print("Aus ");
#endif
      digitalWrite(o_Table[payload[2] - 0x30].o_output, LOW);
      break;
    case 'o':
#ifdef DEBUG
      Serial.print("Puls ");
#endif
      digitalWrite(o_Table[payload[2] - 0x30].o_output, HIGH);
      delay(500);
      digitalWrite(o_Table[payload[2] - 0x30].o_output, LOW);
      break;
  }
#ifdef DEBUG
  Serial.println();
  Serial.println();
#endif
}

EthernetClient ethClient;
PubSubClient client(ethClient);

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
#ifdef DEBUG
    Serial.print("Attempting MQTT connection...");
#endif
    // Attempt to connect
    if (client.connect("arduinoClient")) {
#ifdef DEBUG
      Serial.println("connected");
#endif
      // Once connected, publish an announcement...
      client.publish("fhem/flur/klingel/set", "hello world");
      // ... and resubscribe
      client.subscribe("garage/output");
    } else {
#ifdef DEBUG
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
#endif
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
#ifdef DEBUG
  Serial.begin(57600);
#endif
  client.setServer(server, 1883);
  client.setCallback(callback);

  // I/O-Setup inputs
  for (int i = 0; i < 8; i++) {
    pinMode(i_Table[i].i_input, INPUT_PULLUP);
  }
  // I/O-Setup outputs
  for (int i = 0; i < 8; i++) {
    pinMode(o_Table[i].o_output, OUTPUT);
  }
#ifdef DEBUG
  // Show Array
  for (int i = 0; i < 8; i++) {
    Serial.print(i_Table[i].i_new);
    Serial.print(" ");
    Serial.print(i_Table[i].i_old);
    Serial.print(" ");
    Serial.print(i_Table[i].i_reading);
    Serial.print(" ");
    Serial.print(i_Table[i].i_input);
    Serial.print(" ");
    Serial.print(i_Table[i].i_debounce);
    Serial.println();
  }
#endif
  Ethernet.begin(mac, ip);
  // Allow the hardware to sort itself out
  delay(1500);

  // Initialize 1-Wire-Bus
  ds.reset_search();
  if ( !ds.search(addr)) {
    ds.reset_search();
  }
}


void loop()
{
  currentMillis = millis();
  if (!client.connected()) {
    reconnect();
  }
  // Read Digital In
  inputHandling();

  // Read 1 Wire Sensor (every interval milliseconds)
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    temp = gettemp(); // Temperature aus dem 1Wire lesen
    sprintf(str_temp, "%i", temp);
    client.publish("garage/temp/set", str_temp);
  }
  client.loop();
}

int gettemp() {
  int temp;
  byte present = 0;
  byte data[12];

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end

  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // Read Scratchpad

  //  Serial.print("t=");
  for (int i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }
  temp = data[1];
  temp = temp << 8;
  temp = temp + data[0];
  temp = temp / 16;
  //Serial.println(temp);
  //  Serial.print(" CRC=");
  //  Serial.print( OneWire::crc8( data, 8), HEX);
  //  Serial.println();
  return temp;
}

void inputHandling() {
  for (int i = 0; i < 8; i++) {
    //input 1
    i_Table[i].i_reading = digitalRead(i_Table[i].i_input); // Read input

    if (i_Table[i].i_reading != i_Table[i].i_old) {
      // reset the debouncing timer
      i_Table[i].i_debounce = millis();
    }
    if ((millis() - i_Table[i].i_debounce) > debounceDelay) {
      // whatever the reading is at, it's been there for longer
      // than the debounce delay, so take it as the actual current state:
      if (i_Table[i].i_reading != i_Table[i].i_new) {
        i_Table[i].i_new = i_Table[i].i_reading;
#ifdef DEBUG
        Serial.print("Statuswechsel Input ");
        Serial.print(i);
        Serial.print(" erkannt:");
        Serial.println(i_Table[i].i_new);
#endif
        inputTopic[12] = 0x30 + i;
        if (i_Table[i].i_new) {
          client.publish(inputTopic, "reset");
        }
        else
        {
          client.publish(inputTopic, "set");
        }
      }
    }
    i_Table[i].i_old = i_Table[i].i_reading;
  }
}

