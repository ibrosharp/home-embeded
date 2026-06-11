#include <Arduino.h>
#include <IRremote.hpp>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <WiFi.h>
#include <WebServer.h>

#define IR_RECEIVE_PIN  7
#define IR_SEND_PIN    10
#define PIR_PIN         4
#define DHTPIN          6
#define I2C_SDA         1
#define I2C_SCL         3
#define INT_A_PIN      18
#define INT_B_PIN      19
#define LED_INDICATOR   5

#define MCP_RELAY_1    0  //GPA0
#define MCP_RELAY_2    1  //GPA1
#define MCP_SWITCH_3    2  //GPA2
#define MCP_SWITCH_4    3  //GPA3
#define MCP_SWITCH_5    4  //GPA4
#define MCP_SWITCH_6    5  //GPA5
#define MCP_RELAY_4    6  //GPA6
#define MCP_RELAY_3    7  //GPA7
#define MCP_SWITCH_9    8  //GPB0
#define MCP_SWITCH_10   9  //GPB1
#define MCP_SWITCH_11   10 //GPB2
#define MCP_SWITCH_12   11 //GPB3
#define MCP_SWITCH_1    12 //GPB4
#define MCP_SWITCH_2     13 //GPB5
#define MCP_SWITCH_8     14 //GPB6
#define MCP_SWITCH_7     15 //GPB7

#define DHTTYPE DHT22 

#define DEFAULT_SSID "home_controller"
#define DEFAULT_PASSWORD "qwertyuiop"

uint8_t switch1State;
uint8_t switch2State;
uint8_t switch3State;
uint8_t switch4State;
uint8_t switch5State;
uint8_t switch6State;
uint8_t switch7State;
uint8_t switch8State;
uint8_t switch9State;
uint8_t switch10State;
uint8_t switch11State;
uint8_t switch12State;
uint8_t relay1State;
uint8_t relay2State;
uint8_t relay3State;
uint8_t relay4State;

struct IRCommand {
  decode_type_t protocol;
  uint16_t address;
  uint16_t command;
  uint8_t numberOfBits;
  bool isValid = false;
};

WebServer server(80);

TaskHandle_t IRTaskHandle = NULL;
TaskHandle_t WebTaskHandle = NULL;

void IRProcessorTask(void * pvParameters) {
    Serial.print("IR Task running on core: ");
    Serial.println(xPortGetCoreID());

    IrReceiver.begin(IR_RECEIVE_PIN, LED_INDICATOR);

    // Tasks in FreeRTOS must run in an infinite loop and never return
    for(;;) {
        // Event check
        if (IrReceiver.decode()) {
            Serial.print("[TASK 1] IR Command Received: 0x");
            Serial.println(IrReceiver.decodedIRData.command, HEX);
            
            IrReceiver.resume(); // Clear buffer
        }

        // Yield control back to the OS for 50 milliseconds to let other tasks run
        // This replaces delay() and consumes ZERO CPU power while waiting
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
