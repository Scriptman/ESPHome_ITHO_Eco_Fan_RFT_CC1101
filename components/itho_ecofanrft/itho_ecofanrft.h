#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/fan/fan_state.h"
#include "esphome/components/spi/spi.h"

#include "cc1101.h"
#include "itho_cc1101.h"

namespace esphome {
namespace itho_ecofanrft {

class IthoEcoFanRftComponent;

struct IthoEcoFanRftComponentStore {
    volatile bool data_available;
    volatile uint8_t count;
    ISRInternalGPIOPin *pin;

    void reset();
    static void gpio_intr(IthoEcoFanRftComponentStore *arg);
};


class IthoEcoFanRftFan : public fan::FanState {
 public:
  IthoEcoFanRftFan(IthoEcoFanRftComponent *parent) : parent_(parent) {};
  //void setup() override;
  //void loop() override;
  //void dump_config() override;
  //float get_setup_priority() const override;

 protected:
  IthoEcoFanRftComponent *parent_;
};

class IthoEcoFanRftComponent : public Component,
                               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                     spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {

 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_mosi_pin(GPIOPin *mosi) { mosi_ = mosi; }
  void set_irq_pin(GPIOPin *irq) { irq_ = irq; }
  void set_rf_address(uint64_t address) {
      this->rf_address_.resize(3, 0);
      for (uint8_t i = 0; i < 3; i++) {
          this->rf_address_[i] = (address >> (8 * (2 - i))) & 0xff;
      }
  }
  void set_peer_rf_address(uint64_t address) {
      this->peer_rf_address_.resize(3, 0);
      for (uint8_t i = 0; i < 3; i++) {
          this->peer_rf_address_[i] = (address >> (8 * (2 - i))) & 0xff;
      }
  }

  IthoEcoFanRftFan *get_fan() {
      auto f = new IthoEcoFanRftFan(this);
      this->fan_ = f;
      return f;
  }

  void join();
  void send_command(std::string command);


 protected:
  std::string format_addr_(std::vector<uint8_t> addr);

  void schedule_send_packet_();

  IthoEcoFanRftFan *fan_;
  GPIOPin *mosi_;
  GPIOPin *irq_;

  std::vector<uint8_t> rf_address_;
  std::vector<uint8_t> peer_rf_address_;

  IthoEcoFanRftComponentStore store_;

  CC1101 *cc1101_{nullptr};
  IthoCC1101 *itho_cc1101_{nullptr};

  bool next_update_{true};
};
template<typename... Ts> class JoinAction : public Action<Ts...> {
 public:
  explicit JoinAction(IthoEcoFanRftComponent *component) : component_(component) {}

  void play(Ts... x) override {
    this->component_->join();
  }
 protected:
  IthoEcoFanRftComponent *component_;
};

}  // namespace itho_ecofanrft
}  // namespace esphome