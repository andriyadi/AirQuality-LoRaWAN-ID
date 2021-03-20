#include <Arduino.h>

#include <lorawan.h>

#include "SparkFun_SGP30_Arduino_Library.h" 
#include <Wire.h>
#include <EEPROM.h>

struct SGP30Baseline
{
    uint16_t CO2;
    uint16_t TVOC;
    uint8_t valid = 0;
    uint8_t version = 1;
};

#define EEPROM_START_ADDR               0
#define SGP30_SAVE_BASELINE_INTERVAL    (LORA_TX_INTERVAL/60000)
#define USE_RUNNING_AVG                 1
#define LORA_TX_INTERVAL                (20000) // Interval for testing
// #define LORA_TX_INTERVAL                (60000)
#define USER_LED_PIN                    PA8

// OTAA credentials
const char *devEui = "3ad6a56d88aafcfe";
const char *appEui = "0000000000000001";
const char *appKey = "bcc9494dadf453084d315a28f54c502e";

// Pin-mapping for DycodeX LoRa board that's based on RAK811. Other RAK811-based board can use it
const sRFM_pins RFM_pins = {
    .CS = RADIO_NSS,     //PA7,
    .RST = RADIO_RESET,  //PB13,
    .DIO0 = RADIO_DIO_0, //PA11,
    .DIO1 = RADIO_DIO_1, //PB1,
    .DIO2 = RADIO_DIO_2, //PA3
};

// SGP30 sensor object
SGP30 sgp30; 
SGP30Baseline sgp30CurrBaseline = {};
uint32_t sgp30SaveBaselineCounter = 0;

#if USE_RUNNING_AVG
#define SENSOR_MEAS_INTERVAL  (2000)
#include "RunningAverage.h"
RunningAverage raCO2(10);
RunningAverage raTVOC(10);
uint32_t lastSensorMeasMillis = 0;
#endif

uint16_t lastCO2 = 400;
uint16_t lastTVOC = 0;

// LoRa-related
unsigned long previousLoRaTxMillis = 0;     // will store last time message sent
unsigned int loraTxCounter = 0;             // message counter
char txData[8];
char rxData[255];
byte rxDataLength = 0;
int loraPort, loraChannel, loraFreq;

void initSensor()
{
    Wire.begin();
    // Initialize sensor
    if (sgp30.begin() == false)
    {
        Serial.println("No SGP30 Detected. Check connections.");
        while (1)
            ;
    }
    else
    {
        sgp30.getSerialID();
        Serial.printf("Sensor serial ID: %lu\r\n", sgp30.serialID);
    }

    // Initializes sensor for air quality readings
    // measureAirQuality should be called in one second increments after a call to initAirQuality
    sgp30.initAirQuality();

    // Get baseline from EEPROM or other persistent storage
    EEPROM.get(EEPROM_START_ADDR, sgp30CurrBaseline);
    Serial.printf("Sensor baseline -> CO2: %d, TVOC: %d, version: %d, valid: %d\r\n", sgp30CurrBaseline.CO2, sgp30CurrBaseline.TVOC, sgp30CurrBaseline.version, sgp30CurrBaseline.valid);
    if (sgp30CurrBaseline.valid)
    {
        Serial.println("Use stored baseline");
        sgp30.setBaseline(sgp30CurrBaseline.CO2, sgp30CurrBaseline.TVOC);
        sgp30.getBaseline(); //re-retrieve baseline
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000); // Wait for Serial monitor to open 
    Serial.println("It begins");

    // Activate the MCU clock
    pinMode(RADIO_XTAL_EN, OUTPUT);
    digitalWrite(RADIO_XTAL_EN, 1);
    pinMode(RADIO_RF_CRX_RX, OUTPUT);
    digitalWrite(RADIO_RF_CRX_RX, 1);
    // Control LoRa send by PA_BOOST
    pinMode(RADIO_RF_CTX_PA, OUTPUT);
    digitalWrite(RADIO_RF_CTX_PA, 1); 

    pinMode(USER_LED_PIN, OUTPUT);
    digitalWrite(USER_LED_PIN, HIGH);

    Serial.println("Initializing sensor...");
    initSensor();

    Serial.println("Initializing LoRaWAN module...");
    if (!lora.init())
    {
        Serial.println("LoRaWAN module not detected!");
        digitalWrite(USER_LED_PIN, LOW);
        // Do nothing
        while(1)
            ;
    }

    // Set LoRaWAN Class change CLASS_A or CLASS_C
    lora.setDeviceClass(CLASS_A);

    // Set Data Rate
    lora.setDataRate(SF10BW125);

    // Set channel to random
    lora.setChannel(MULTI);

    // Set TxPower to 15 dBi 
    lora.setTxPower(15);

    // Put OTAA Key and DevAddress here
    lora.setDevEUI(devEui);
    lora.setAppEUI(appEui);
    lora.setAppKey(appKey);

    // Join procedure
    bool isJoined;
    do
    {
        Serial.println("Joining...");
        isJoined = lora.join();

        // Wait for 10s to retry joining
        delay(10000);
    } while (!isJoined);
    Serial.println("Joined to LoRaWAN network");

    digitalWrite(USER_LED_PIN, LOW);
}

void loop()
{
#if USE_RUNNING_AVG
    // Check if need to read sensor and do running averaging
    if (millis() - lastSensorMeasMillis > SENSOR_MEAS_INTERVAL) {
        lastSensorMeasMillis = millis();

        sgp30.measureAirQuality();

        raCO2.addValue(sgp30.CO2);
        raTVOC.addValue(sgp30.TVOC);

        lastCO2 = roundf(raCO2.getAverage());
        lastTVOC = roundf(raTVOC.getAverage());

        Serial.printf("CO2: %d, TVOC: %d\r\n", lastCO2, lastTVOC);
    }
#endif

    // Check TX interval 
    if (millis() - previousLoRaTxMillis > LORA_TX_INTERVAL)
    {
        previousLoRaTxMillis = millis();

#if !USE_RUNNING_AVG
        // Read sensor
        sgp30.measureAirQuality();
        lastCO2 = sgp30.CO2;
        lastTVOC = sgp30.TVOC;
#endif

        int actLen = 0;
        
        if (loraTxCounter % 2 == 0) {
            // CO2
            actLen = snprintf(txData, 8, "%d", lastCO2);
            txData[actLen] = 0;
            Serial.printf("Sending CO2: %s\r\n", txData);
            lora.sendUplink(txData, strlen(txData), 0, 2);

            loraTxCounter++;
            loraPort = lora.getFramePortTx();
            loraChannel = lora.getChannel();
            loraFreq = lora.getChannelFreq(loraChannel);
            Serial.printf("fport: %d. Ch: %d, Freq: %d\r\n", loraPort, loraChannel, loraFreq);
        }
        else {
            
            // TVOC
            actLen = snprintf(txData, 8, "%d", lastTVOC);
            txData[actLen] = 0;
            Serial.printf("Sending TVOC: %s\r\n", txData);
            lora.sendUplink(txData, strlen(txData), 0, 3);

            loraTxCounter++;
            loraPort = lora.getFramePortTx();
            loraChannel = lora.getChannel();
            loraFreq = lora.getChannelFreq(loraChannel);
            Serial.printf("fport: %d. Ch: %d, Freq: %d\r\n", loraPort, loraChannel, loraFreq);
        }

        // Saving baseline
        if ((uint32_t)(++sgp30SaveBaselineCounter % SGP30_SAVE_BASELINE_INTERVAL) == (uint32_t)(SGP30_SAVE_BASELINE_INTERVAL-1))
        {
            if (sgp30.getBaseline() == SGP30_SUCCESS)
            {
                Serial.printf("Do saving baseline... %d, %d\r\n", sgp30.baselineCO2, sgp30.baselineTVOC);
                sgp30CurrBaseline.CO2 = sgp30.baselineCO2;
                sgp30CurrBaseline.TVOC = sgp30.baselineTVOC;
                sgp30CurrBaseline.valid = 1;
                EEPROM.put(EEPROM_START_ADDR, sgp30CurrBaseline);
            }
        }
    }

    // Check Lora RX
    lora.update();

    rxDataLength = lora.readData(rxData);

    if (rxDataLength)
    {
        digitalWrite(USER_LED_PIN, HIGH);

        int rxDataCounter = 0;
        loraPort = lora.getFramePortRx();
        loraChannel = lora.getChannelRx();
        loraFreq = lora.getChannelRxFreq(loraChannel);

        for (int i = 0; i < rxDataLength; i++)
        {
            if (((rxData[i] >= 32) && (rxData[i] <= 126)) || (rxData[i] == 10) || (rxData[i] == 13))
                rxDataCounter++;
        }

        if (loraPort != 0)
        {
            if (rxDataCounter == rxDataLength)
            {
                Serial.print(F("Received String: "));
                for (int i = 0; i < rxDataLength; i++)
                {
                    Serial.print(char(rxData[i]));
                }
            }
            else
            {
                Serial.print(F("Received Hex: "));
                for (int i = 0; i < rxDataLength; i++)
                {
                    Serial.print(rxData[i], HEX);
                    Serial.print(" ");
                }
            }
            Serial.println();
            Serial.print(F("fport: "));
            Serial.print(loraPort);
            Serial.print(" ");
            Serial.print(F("Ch: "));
            Serial.print(loraChannel);
            Serial.print(" ");
            Serial.print(F("Freq: "));
            Serial.println(loraFreq);
            Serial.println(" ");

            delay(300);
            digitalWrite(USER_LED_PIN, LOW); // Let user know we have new data
        }
        else
        {
            Serial.print(F("Received Mac Cmd : "));
            for (int i = 0; i < rxDataLength; i++)
            {
                Serial.print(rxData[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
            Serial.print(F("fport: "));
            Serial.print(loraPort);
            Serial.print(" ");
            Serial.print(F("Ch: "));
            Serial.print(loraChannel);
            Serial.print(" ");
            Serial.print(F("Freq: "));
            Serial.println(loraFreq);
            Serial.println(" ");

            digitalWrite(USER_LED_PIN, LOW);
        }
    }
}