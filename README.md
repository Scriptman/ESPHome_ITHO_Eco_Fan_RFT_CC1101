# ESPHome ITHO CVE ECO-FAN 2 control
Library for NodeMCU ESP8266 in combination with Hassio Home Assistant ESPHome ITHO Eco Fan CC1101 (Including older Eco fans!)


Trying to get ESPHome to mimic what is comprised in
 
 - https://github.com/jodur/ESPEASY_Plugin_ITHO/blob/master/_P145_Itho.ino
 - https://github.com/adri/IthoEcoFanRFT / https://github.com/supersjimmie/IthoEcoFanRFT
 
Code is optimized for Itho CVE Eco-fan 2. For newer fans, please see the IthoCC1101.cpp file and search for "> 2011" and make the changes as described. Not 100% tested.


## Wiring schema used:

```
Connections between the CC1101 and the ESP8266 or Arduino:
CC11xx pins    ESP pins Arduino pins  Description
*  1 - VCC        VCC      VCC           3v3
*  2 - GND        GND      GND           Ground
*  3 - MOSI       13=D7    Pin 11        Data input to CC11xx
*  4 - SCK        14=D5    Pin 13        Clock pin
*  5 - MISO/GDO1  12=D6    Pin 12        Data output from CC11xx / serial clock from CC11xx
*  6 - GDO2       04=D1    Pin  2        Programmable output
*  7 - GDO0       ?        Pin  ?        Programmable output
*  8 - CSN        15=D8    Pin 10        Chip select / (SPI_SS)
```


### Software used
Install the ESPHome addon for Home Assistant. I like to use the program ESPHome flasher on my laptop for flashing the firmware on my NodeMCU.

## Prepairing your Home Assistant for the ITHO controller
Open your `configuration.yaml` file and insert the following lines of code: (I like to put this code into fans.yaml and insert `fan: !include fans.yaml` in my configuration.yaml file)
```
fan:
  - platform: template
    fans:
      mechanical_ventilation:
        friendly_name: "Mechanical Ventilation"
        value_template: >
          {{ "off" if states('sensor.fanspeed') == 'Low' else "on" }}
        speed_template: "{{ states('sensor.fanspeed') }}"
        turn_on:
          service: switch.turn_on
          data:
            entity_id: switch.fansendhigh
        turn_off:
          service: switch.turn_on
          data:
            entity_id: switch.fansendlow
        set_speed:
          service: switch.turn_on
          data_template:
            entity_id: >
              {% set mapper = { 'Timer 1':'switch.fansendtimer1','Timer 2':'switch.fansendtimer2','Timer 3':'switch.fansendtimer3','High':'switch.fansendhigh', 'Medium':'switch.fansendmedium', 'Low':'switch.fansendlow' } %}
              {{ mapper[speed] if speed in mapper else switch.fansendlow }}
        speeds:
          - 'Low'
          - 'Medium'
          - 'High'
          - 'Timer 1'
          - 'Timer 2'
          - 'Timer 3'
```

## ESPHome Configuration
I created a new device in Home Assistant ESPHome addon (named itho_eco_fan), and choose patform "ESP8266" and board "d1_mini_pro". After that, I changed the YAML of that device to look like this:

```
esphome:
  name: itho_eco_fan
  platform: ESP8266
  board: d1_mini_pro
  includes: 
    - itho_eco_fan/itho/c1101.h
  libraries: 
    - https://github.com/Scriptman/ESPHome_ITHO_Eco_Fan_CC1101.git

wifi:
  ssid: "WiFi Network To Connect To"
  password: "WiFi_Password123#"

  # Enable fallback hotspot (captive portal) in case wifi connection fails
  ap:
    ssid: "Itho Eco Fan Fallback Hotspot"
    password: "Generated_password"

captive_portal:

# Enable logging
logger:

# Enable Home Assistant API
api:
  password: "api_password_for_security"

ota:
  password: "ota_password_for_security"
  
switch:
- platform: custom
  lambda: |-
    auto fansendlow = new FanSendLow();
    App.register_component(fansendlow);
    return {fansendlow};
  switches:
    name: "FanSendLow"
    icon: mdi:fan

- platform: custom
  lambda: |-
    auto fansendmedium = new FanSendMedium();
    App.register_component(fansendmedium);
    return {fansendmedium};
  switches:
    name: "FanSendMedium"
    icon: mdi:fan

- platform: custom
  lambda: |-
    auto fansendhigh = new FanSendHigh();
    App.register_component(fansendhigh);
    return {fansendhigh};
  switches:
    name: "FanSendHigh"
    icon: mdi:fan

- platform: custom
  lambda: |-
    auto fansendt1 = new FanSendIthoTimer1();
    App.register_component(fansendt1);
    return {fansendt1};
  switches:
    name: "FanSendTimer1"

- platform: custom
  lambda: |-
    auto fansendt2 = new FanSendIthoTimer2();
    App.register_component(fansendt2);
    return {fansendt2};
  switches:
    name: "FanSendTimer2"

- platform: custom
  lambda: |-
    auto fansendt3 = new FanSendIthoTimer3();
    App.register_component(fansendt3);
    return {fansendt3};
  switches:
    name: "FanSendTimer3"

- platform: custom
  lambda: |-
    auto fansendjoin = new FanSendIthoJoin();
    App.register_component(fansendjoin);
    return {fansendjoin};
  switches:
    name: "FanSendJoin"

# Rinse/repeat for the timers
# see outstanding question in c1101.h
# on multiple switches handling

text_sensor:
- platform: custom
  lambda: |-
    auto fanrecv = new FanRecv();
    App.register_component(fanrecv);
    return {fanrecv->fanspeed,fanrecv->fantimer};
  text_sensors:
    - name: "FanSpeed"
    - name: "FanTimer"
```
