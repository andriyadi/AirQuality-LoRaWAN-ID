# AirQuality-LoRaWAN-ID
A simple demo to work with LoRaWAN that's compliant with Indonesia region. It reads air quality data (CO2 and TVOC) from SGP30 sensor, and transmit them over LoRaWAN.

For this project, I need to fork and modify a LoRaWAN library and posted it here: [https://github.com/andriyadi/Beelan-LoRaWAN](https://github.com/andriyadi/Beelan-LoRaWAN). By that, the LoRaWAN regional parameters complies with Indonesia's LPWA tech spec regulation, and the device may be able to connect with LoRaWAN public network in Indonesia, e.g: [Antares](https://antares.id) platform and network server. The library is originally from [here](https://github.com/BeelanMX/Beelan-LoRaWAN). 

## Demo setup
![Demo setup](https://github.com/andriyadi/AirQuality-LoRaWAN-ID/raw/main/assets/demo.jpeg)

### Hardware
If you want to replicate this project without any modification, you need following hardware:
* DycodeX LoRa development board (contact me to get one). Or you can use any boards using RAK811 module but you may need to adjust some minor code.
* SGP30 sensor module. I use the one from Seeed Studio
* Optionally ST-Link for flashing the firmware and debugging. I set the PlatformIO configuration file to use ST-Link. If you want to use DFU or JLink uploader, adjust the config (.ini) file

### Software
* This project is developed using PlatformIO tooling. Make sure to install it.
* ST STM32 platform to work with STM32 MCU which powers the board. I use Arduino framework, which is based on STM32Duino project.

## LoRaWAN Network
To publish data over LoRaWAN, obviously you need a LoRaWAN gateway. For this project, the gateway should be tuned to frequency plan for Indonesia.
Or you can use public LoRaWAN network, such as Antares. Try to register and login, read their documentation.

## Run the Project
In `src/main_lora_iaq.hpp`, adjust these constants according to your OTAA credentials:
```
// OTAA credentials
const char *devEui = "<CHANGE_THIS>";
const char *appEui = "<CHANGE_THIS>";
const char *appKey = "<CHANGE_THIS>";
```

## Credits
* [Beelan-LoRaWAN](https://github.com/BeelanMX/Beelan-LoRaWAN)
* Antares sample code
