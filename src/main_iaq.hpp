#include "Arduino.h"
#include "SparkFun_SGP30_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_SGP30
#include <Wire.h>
#include <EEPROM.h>

struct SGP30Baseline {
  uint16_t CO2;
  uint16_t TVOC;
  uint8_t valid = 0;
  uint8_t version = 1;
};

#define EEPROM_START_ADDR       0
#define SENSOR_MEAS_INTERVAL   (2000)
#define USE_RUNNING_AVG        0//1

SGP30 mySensor; //create an object of the SGP30 class
SGP30Baseline currentBaseline = {};
uint32_t lastSensorMeasMillis = 0;
uint16_t lastCO2 = 400;
uint16_t lastTVOC = 0;

#if USE_RUNNING_AVG
#include "RunningAverage.h"
RunningAverage raCO2(10);
RunningAverage raTVOC(10);
#endif

void setup()
{
    delay(1000);
    Serial.begin(115200);

    Serial.println("It begins...");

    Wire.begin();
    // Initialize sensor
    if (mySensor.begin() == false)
    {
        Serial.println("No SGP30 Detected. Check connections.");
        while (1)
            ;
    }
    else
    {
        mySensor.getSerialID();
        Serial.printf("Sensor serial ID: %lu\r\n", mySensor.serialID);
    }

    // Initializes sensor for air quality readings
    // measureAirQuality should be called in one second increments after a call to initAirQuality
    mySensor.initAirQuality();
    lastSensorMeasMillis = millis(); // So next measurement will be some interval after this

    // Get baseline from EEPROM or other persistent storage
    EEPROM.get(EEPROM_START_ADDR, currentBaseline);
    Serial.printf("Sensor baseline -> CO2: %d, TVOC: %d, version: %d, valid: %d\r\n", currentBaseline.CO2, currentBaseline.TVOC, currentBaseline.version, currentBaseline.valid);
    if (currentBaseline.valid) {
        Serial.println("Use stored baseline");
        mySensor.setBaseline(currentBaseline.CO2, currentBaseline.TVOC);
        mySensor.getBaseline(); //re-retrieve baseline
    }
}

uint32_t saveBaselineCounter = 0;

void loop()
{
    // First fifteen readings will be
    // CO2: 400 ppm  TVOC: 0 ppb
    
    if (millis() - lastSensorMeasMillis > SENSOR_MEAS_INTERVAL) {
        lastSensorMeasMillis = millis();

        // measure CO2 and TVOC levels
        mySensor.measureAirQuality();
        
#if USE_RUNNING_AVG
        raCO2.addValue(mySensor.CO2);
        raTVOC.addValue(mySensor.TVOC);

        lastCO2 = roundf(raCO2.getAverage());
        lastTVOC = roundf(raTVOC.getAverage());
#else
        lastCO2 = mySensor.CO2;
        lastTVOC = mySensor.TVOC;
#endif

        Serial.printf("CO2: %d ppm, TVOC: %d ppb\r\n", lastCO2, lastTVOC);

        // Check if we need to store baseline
        if (++saveBaselineCounter % 30 == 29) {
            Serial.println("Saving baseline...");
            if (mySensor.getBaseline() == SGP30_SUCCESS) {
                //Saving
                Serial.printf("Do saving baseline... %d, %d\r\n", mySensor.baselineCO2, mySensor.baselineTVOC);
                currentBaseline.CO2 = mySensor.baselineCO2;
                currentBaseline.TVOC = mySensor.baselineTVOC;
                currentBaseline.valid = 1;
                EEPROM.put(EEPROM_START_ADDR, currentBaseline);
            }
        }
    }
}