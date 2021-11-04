#include "esphome/core/log.h"
#include "itho_ecofanrft.h"
#include "cc1101.h"
#include "itho_cc1101_protocol.h"


namespace esphome {
namespace itho_ecofanrft {

static const char *TAG = "itho_ecofanrft.component";

void ICACHE_RAM_ATTR IthoEcoFanRftComponentStore::gpio_intr(IthoEcoFanRftComponentStore *arg) {
  arg->data_available = true;
  arg->count = (arg->count + 1) % 0xFF;
}
void ICACHE_RAM_ATTR IthoEcoFanRftComponentStore::reset() {
  data_available = false;
}

std::string IthoEcoFanRftComponent::format_addr_(std::vector<uint8_t> addr) {
  std::string s;
  char buf[20];
  for (uint8_t a : addr) {
      sprintf(&buf[0], "%02X:", a);
      s += buf;
  }
  s.pop_back();
  return s;
}

void itho_ecofanrft::IthoEcoFanRftComponent::dump_config() {
  std::string s;

  ESP_LOGCONFIG(TAG, "Itho EcoFanRft '%s'", this->fan_->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  RF Address: '%s'", this->format_addr_(this->rf_address_).c_str());
  if (this->peer_rf_address_.size() > 0) {
    ESP_LOGCONFIG(TAG, "  RF Peer Address: '%s'", this->format_addr_(this->peer_rf_address_).c_str());
  }
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  IRQ Pin: ", this->irq_);

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  std::vector<uint8_t> config = this->cc1101_->read_burst_register(0x00, 47);
  for (uint8_t i = 0; i < config.size(); i++) {
    ESP_LOGCONFIG(TAG, "Config register [%02X] => [%02X]", i, config[i]);
  }
#endif
}
void IthoEcoFanRftComponent::setup() {

  if (this->cc1101_ == nullptr) {
    this->cc1101_ = new CC1101(this, this->parent_->get_miso(), this->cs_);
  }

  if (this->itho_cc1101_ == nullptr) {
    this->itho_cc1101_ = new IthoCC1101(this->cc1101_, this->rf_address_);
  }

  this->spi_setup();

  if (!this->cc1101_->init()) {
    this->mark_failed();
    return;
  }

  // Setup module for Itho protocol
  this->itho_cc1101_->init_itho();

  // Enable interrupt on packet in RX FIFO
  this->store_.data_available = false;
  this->store_.count = 0;
  this->store_.pin = this->irq_->to_isr();
  this->irq_->attach_interrupt(IthoEcoFanRftComponentStore::gpio_intr, &this->store_, RISING);

  auto traits = fan::FanTraits(false, true);    // No oscillating, just speed
  this->fan_->set_traits(traits);
  this->fan_->add_on_state_callback([this]() { this->next_update_ = true; });

  // Set CC1101 in receive mode
  this->itho_cc1101_->enable_receive_mode();
}
void IthoEcoFanRftComponent::loop() {

  if (this->store_.data_available) {

    this->store_.reset();

    int16_t rssi = this->cc1101_->read_rssi();

    ESP_LOGD(TAG, "Data available in RX FIFO! (%02x) (%4d dBm)", this->store_.count, rssi);

    {
        uint8_t speed;
        if (this->itho_cc1101_->get_fan_speed(this->peer_rf_address_, &speed)) {

            auto call = this->fan_->make_call();

            if (speed > 0x00) {
                call.set_state(true);

                if (speed < 0x40) {
                    call.set_speed(fan::FAN_SPEED_LOW);
                } else if (speed < 0x80) {
                    call.set_speed(fan::FAN_SPEED_MEDIUM);
                } else {
                    call.set_speed(fan::FAN_SPEED_HIGH);
                }
            } else {
                call.set_state(false);
            }
            call.perform();

            // next_update should not run after RF status update
            this->next_update_ = false;
        }
    }
    this->itho_cc1101_->enable_receive_mode();
  }


  if (!this->next_update_) {
    return;
  }
  this->next_update_ = false;

  {
    std::string speed;

    if (this->fan_->state) {
      if (this->fan_->speed == fan::FAN_SPEED_LOW)
        speed = "low";
      else if (this->fan_->speed == fan::FAN_SPEED_MEDIUM)
        speed = "medium";
      else if (this->fan_->speed == fan::FAN_SPEED_HIGH)
        speed = "high";
    }
    ESP_LOGD(TAG, "Setting speed: '%s'", speed.c_str());

    bool enable = this->fan_->state;
    if (enable) {
        ESP_LOGD(TAG, "Sending speed: '%s'", speed.c_str());
        this->send_command(speed);
    } else {
        ESP_LOGD(TAG, "Sending speed: min");
        this->send_command("min");
    }

    ESP_LOGD(TAG, "Setting itho_ecofanrft state: %s", ONOFF(enable));
  }
}
float IthoEcoFanRftComponent::get_setup_priority() const { return setup_priority::DATA; }

void IthoEcoFanRftComponent::join() {
  ESP_LOGD(TAG, "Fan '%s': join() called", this->fan_->get_name().c_str());
  this->send_command("join");
}

void IthoEcoFanRftComponent::send_command(std::string command) {

    this->itho_cc1101_->send_command(command);

    // After sending command switch back to receive mode
    this->itho_cc1101_->enable_receive_mode();

    this->set_timeout("send_command", 40, [this]() {
            this->schedule_send_packet_();
    });
}

void IthoEcoFanRftComponent::schedule_send_packet_() {

    uint8_t tries_left =  this->itho_cc1101_->send_packet();

    // After sending packet switch back to receive mode
    this->itho_cc1101_->enable_receive_mode();

    if (tries_left > 0) {
        this->set_timeout("send_command", 50, [this]() {
                this->schedule_send_packet_();
        });
    }
}

} // namespace itho_ecofanrft
}  // namespace esphome