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
    - itho_eco_fan/itho/cc1101.h
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
# see outstanding question in cc1101.h
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

## Add the cc1101.h file (interface class to the CC1101 library)
You're almost done! With the "Home Assistant Configurator" I navigate to the folder `esphome/itho_eco_fan`. Create the folder `itho` and go into the folder. Create the file `cc1101.h` and add the following contents to the file:

```
#include "esphome.h"
#include "IthoCC1101.h"
#include "IthoPacket.h"
#include "Ticker.h"

IthoCC1101 rf;
void ITHOinterrupt() ICACHE_RAM_ATTR;
void ITHOcheck();

// extra for interrupt handling
bool ITHOhasPacket = false;
Ticker ITHOticker;
String State="Low"; // after startup it is assumed that the fan is running low
String OldState="Low";
int Timer=0;
int LastIDindex = 0;
int OldLastIDindex = 0;
long LastPublish=0; 
bool InitRunned = false;

// Timer values for hardware timer in Fan
#define Time1      10*60
#define Time2      20*60
#define Time3      30*60


class FanRecv : public PollingComponent {
  public:

    // Publish two sensors
    // Speed: the speed the fan is running at (depending on your model 1-2-3 or 1-2-3-4
    TextSensor *fanspeed = new TextSensor();
    // Timer left (though this is indicative) when pressing the timer button once, twice or three times
    TextSensor *fantimer = new TextSensor();

    // For now poll every 15 seconds
    FanRecv() : PollingComponent(15000) { }

    void setup() {
      rf.init();
      // Followin wiring schema, change PIN if you wire differently
      pinMode(D1, INPUT);
      attachInterrupt(D1, ITHOinterrupt, RISING);
      //attachInterrupt(D1, ITHOcheck, RISING);
      rf.initReceive();
    }

    void update() override {
        fanspeed->publish_state(State.c_str());
        fantimer->publish_state(String(Timer).c_str());
    }


};

// Figure out how to do multiple switches instead of duplicating them
// we need
// send: low, medium, high, full
//       timer 1 (10 minutes), 2 (20), 3 (30)
// To optimize testing, reset published state immediately so you can retrigger (i.e. momentarily button press)
class FanSendFull : public Component, public Switch {
  public:

    void write_state(bool state) override {
      if ( state ) {
        rf.sendCommand(IthoFull);
        State = "High";
        Timer = 0;
        publish_state(!state);
      }
    }
};

class FanSendHigh : public Component, public Switch {
  public:

    void write_state(bool state) override {
      if ( state ) {
        rf.sendCommand(IthoHigh);
        State = "High";
        Timer = 0;
        publish_state(!state);
      }
    }
};

class FanSendMedium : public Component, public Switch {
  public:

    void write_state(bool state) override {
      if ( state ) {
        rf.sendCommand(IthoMedium);
        State = "Medium";
        Timer = 0;
        publish_state(!state);
      }
    }
};

class FanSendLow : public Component, public Switch {
  public:

    void write_state(bool state) override {
      if ( state ) {
        rf.sendCommand(IthoLow);
        State = "Low";
        Timer = 0;
        publish_state(!state);
      }
    }
};

class FanSendIthoTimer1 : public Component, public Switch {
  public:

    void write_state(bool state) override {
      if ( state ) {
        rf.sendCommand(IthoTimer1);
        State = "High";
        Timer = Time1;
        publish_state(!state);
      }
    }
};

class FanSendIthoTimer2 : public Component, public Switch {
  public:

    void write_state(bool state) override {
      if ( state ) {
        rf.sendCommand(IthoTimer2);
        State = "High";
        Timer = Time2;
        publish_state(!state);
      }
    }
};

class FanSendIthoTimer3 : public Component, public Switch {
  public:

    void write_state(bool state) override {
      if ( state ) {
        rf.sendCommand(IthoTimer3);
        State = "High";
        Timer = Time3;
        publish_state(!state);
      }
    }
};

class FanSendIthoJoin : public Component, public Switch {
  public:

    void write_state(bool state) override {
      if ( state ) {
        rf.sendCommand(IthoJoin);
        State = "Join";
        Timer = 0;
        publish_state(!state);
      }
    }
};

void ITHOinterrupt() {
	ITHOticker.once_ms(10, ITHOcheck);
}

void ITHOcheck() {
  noInterrupts();
  if (rf.checkForNewPacket()) {
    IthoCommand cmd = rf.getLastCommand();
    switch (cmd) {
      case IthoUnknown:
        ESP_LOGD("custom", "Unknown state");
        break;
      case IthoStandby:
      case DucoStandby:
        ESP_LOGD("custom", "IthoStandby");
      case IthoLow:
      case DucoLow:
        ESP_LOGD("custom", "IthoLow");
        State = "Low";
        Timer = 0;
        break;
      case IthoMedium:
      case DucoMedium:
        ESP_LOGD("custom", "Medium");
        State = "Medium";
        Timer = 0;
        break;
      case IthoHigh:
      case DucoHigh:
        ESP_LOGD("custom", "High");
        State = "High";
        Timer = 0;
        break;
      case IthoFull:
        ESP_LOGD("custom", "Full");
        State = "Full";
        Timer = 0;
        break;
      case IthoTimer1:
        ESP_LOGD("custom", "Timer1");
        State = "High";
        Timer = Time1;
        break;
      case IthoTimer2:
        ESP_LOGD("custom", "Timer2");
        State = "High";
        Timer = Time2;
        break;
      case IthoTimer3:
        ESP_LOGD("custom", "Timer3");
        State = "High 30";
        Timer = Time3;
        break;
      case IthoJoin:
        break;
      case IthoLeave:
        break;
    }
  }
  interrupts();
}
```
Save the file and go back to your device in the Home Assistant ESPHome addon.

## Flashing the NodeMCU and finishing the setup/installation
  1. "VALIDATE" the changes we made by clicking on the "VALIDATE" button/link. If everything is correct, Compile the code and download the BIN file.
  2. Restart Home Assistant to apply the configuration changes you made earlier.
  3. Attach the NodeMCU device with an USB cable to your laptop and start "ESPHome Flasher" with administrator privileges.
  4. Select the correct COM-port and select the BIN file you just created and downloaded.
  5. Flash the device.
  6. If you done everything right, your device should be up and running and connected to your WiFi network. In Home Assistant, navigate to "Settings > Integrations", your device should be found by Home Assistant. Click "Configure" and couple the device to Home Assistant (It will ask for the password you choose for the "API section" in the ESPHome device YAML).
  
Congrats! Your Itho Eco Fan Controller is connected to your Home Assistant, but we need to pair the device with your Itho Eco Fan.

## Pairing with Itho CVE Eco-fan 2
  1. Disconnect the Itho Eco Fan from your power net and wait +/- 30 seconds (This is what I did).
  2. Meanwhile, navigate in Home Assistant to "Settings > Integrations > Itho_eco_fan", you will see a couple of switches. Don't click any yet.
  3. Connect the Itho Eco Fan to your power net.
  4. Within +/- 20 seconds (as soon as you can), click on the switch behind "FanSendJoin".
  
If everything went right, which never happens the first 30 times, then your device should be connected to your Itho Eco Fan. Somethimes the fan goes into mode "2/Medium" when pairing, to let you know the pairing process went right.

## Have fun! Having problems?
https://gathering.tweakers.net/forum/list_messages/1690945
