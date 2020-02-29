# ESPHome_ITHO_Eco_Fan_CC1101
Library for NodeMCU ESP8266 in combination with Hassio Home Assistant ESPHome ITHO Eco Fan CC1101 (Including older Eco fans!)


# ESPHome ITHO CVE ECO-FAN 2 control
Trying to get ESPHome to mimic what is comprised in
 
 - https://github.com/jodur/ESPEASY_Plugin_ITHO/blob/master/_P145_Itho.ino
 - https://github.com/adri/IthoEcoFanRFT / https://github.com/supersjimmie/IthoEcoFanRFT
 
Code is optimized for Itho CVE Eco-fan 2. For newer fans, please see the IthoCC1101.cpp file and search for "> 2011" and make the changes as described. Not 100% tested.


# Wiring schema used:

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


# How-to
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
