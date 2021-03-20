#include <Arduino.h>

#include <lorawan.h>

// OTAA credentials
const char *devEui = "40b6f565cfc17467";
const char *appEui = "0000000000000001";
const char *appKey = "bcc9494dadf453084d315a28f54c502e";

const unsigned long interval = 30000;    // 10 s interval to send message
unsigned long previousMillis = 0;  // will store last time message sent
unsigned int counter = 0;     // message counter

char myStr[50];
byte outStr[255];
byte recvStatus = 0;
int port, channel, freq;
bool newmessage = false;

const sRFM_pins RFM_pins = {
  .CS = RADIO_NSS, //PA7,
  .RST = RADIO_RESET, //PB13,
  .DIO0 = RADIO_DIO_0, //PA11,
  .DIO1 = RADIO_DIO_1, //PB1,
  .DIO2 = RADIO_DIO_2, //PA3
};

void setup() {
  // Setup loraid access
  Serial.begin(115200);
  delay(2000);
  Serial.println("It begins");

  pinMode(RADIO_XTAL_EN, OUTPUT);  
  digitalWrite(RADIO_XTAL_EN, 1);  
  pinMode(RADIO_RF_CRX_RX, OUTPUT);  
  digitalWrite(RADIO_RF_CRX_RX, 1);  
  pinMode(RADIO_RF_CTX_PA,OUTPUT);  
  digitalWrite(RADIO_RF_CTX_PA, 1);  //control LoRa send by PA_BOOST
  
  pinMode(PA8, OUTPUT);
  digitalWrite(PA8, HIGH);

  // SPI.setMISO(RADIO_MISO);
  // SPI.setMOSI(RADIO_MOSI);
  // SPI.setSCLK(RADIO_SCLK);
  // SPI.setSSEL(RADIO_NSS);

  Serial.println("Initting LoRa...");
  if (!lora.init()) {
    Serial.println("RFM95 not detected");
    delay(5000);
    digitalWrite(PA8, LOW);
    return;
  }

  // Set LoRaWAN Class change CLASS_A or CLASS_C
  lora.setDeviceClass(CLASS_A);

  // Set Data Rate
  lora.setDataRate(SF10BW125);

  // Set FramePort Tx
  lora.setFramePortTx(5);

  // set channel to random
  lora.setChannel(MULTI);

  // Set TxPower to 15 dBi (max)
  lora.setTxPower(15);

  // Put OTAA Key and DevAddress here
  lora.setDevEUI(devEui);
  lora.setAppEUI(appEui);
  lora.setAppKey(appKey);

  // Join procedure
  bool isJoined;
  do {
    Serial.println("Joining...");
    isJoined = lora.join();

    //wait for 10s to try again
    delay(10000);
  } while (!isJoined);
  Serial.println("Joined to network");

  digitalWrite(PA8, LOW);
}

void loop() {
  // Check interval overflow
  if (millis() - previousMillis > interval) {
    previousMillis = millis();

    sprintf(myStr, "Lora Counter-%d", counter);

    Serial.print("Sending: ");
    Serial.println(myStr);

    lora.sendUplink(myStr, strlen(myStr), 0);
    counter++;
    port = lora.getFramePortTx();
    channel = lora.getChannel();
    freq = lora.getChannelFreq(channel);
    Serial.print(F("fport: "));    Serial.print(port); Serial.print(" ");
    Serial.print(F("Ch: "));    Serial.print(channel); Serial.print(" ");
    Serial.print(F("Freq: "));    Serial.print(freq); Serial.println(" ");
   
  }

  // Check Lora RX
  lora.update();

  recvStatus = lora.readDataByte(outStr);

  if (recvStatus) {
    digitalWrite(PA8, HIGH);

    newmessage = true;
    int counter = 0;
    port = lora.getFramePortRx();
    channel = lora.getChannelRx();
    freq = lora.getChannelRxFreq(channel);

    for (int i = 0; i < recvStatus; i++)
    {
      if (((outStr[i] >= 32) && (outStr[i] <= 126)) || (outStr[i] == 10) || (outStr[i] == 13))
        counter++;
    }
    if (port != 0)
    {
      if (counter == recvStatus)
      {
        Serial.print(F("Received String : "));
        for (int i = 0; i < recvStatus; i++)
        {
          Serial.print(char(outStr[i]));
        }
      }
      else
      {
        Serial.print(F("Received Hex : "));
        for (int i = 0; i < recvStatus; i++)
        {
          Serial.print(outStr[i], HEX); Serial.print(" ");
        }
      }
      Serial.println();
      Serial.print(F("fport: "));    Serial.print(port); Serial.print(" ");
      Serial.print(F("Ch: "));    Serial.print(channel); Serial.print(" ");
      Serial.print(F("Freq: "));    Serial.println(freq); Serial.println(" ");

      delay(400);
      digitalWrite(PA8, LOW);
    }
    else
    {
      Serial.print(F("Received Mac Cmd : "));
      for (int i = 0; i < recvStatus; i++)
      {
        Serial.print(outStr[i], HEX); Serial.print(" ");
      }
      Serial.println();
      Serial.print(F("fport: "));    Serial.print(port); Serial.print(" ");
      Serial.print(F("Ch: "));    Serial.print(channel); Serial.print(" ");
      Serial.print(F("Freq: "));    Serial.println(freq); Serial.println(" ");
    }
  }
}