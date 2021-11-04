#pragma once

#include "esphome/core/component.h"
#include "cc1101.h"
#include "itho_cc1101_protocol.h"

namespace esphome {
namespace itho_ecofanrft {

class IthoCC1101 {

 public:
  IthoCC1101(CC1101 *cc1101, std::vector<uint8_t> rf_address) : cc1101_(cc1101), rf_address_(rf_address) {};

  void init_itho();
  void enable_receive_mode() {
     this->cc1101_->flush_rxfifo();
     this->cc1101_->receive();
  }
  std::vector<uint8_t> get_data();
  bool get_fan_speed(std::vector<uint8_t> peer_rf_address, uint8_t *speed);
  bool has_valid_checksum(std::vector<uint8_t> data) { return this->calc_checksum(data) == 0x00; }

  uint8_t calc_checksum(std::vector<uint8_t> data);

  void send_command(std::string command);

  uint8_t send_packet();
  void prepare_packet(std::string command);

 protected:

  void manchester_decode_();
  void remove_sync_bits_and_reverse_();

  void manchester_encode_();
  void add_sync_bits_and_reverse_();

  std::vector<uint8_t> packet_;
  std::uint8_t counter_ = 0;
  std::uint8_t tries_ = 0;

  CC1101 *cc1101_{nullptr};
  std::vector<uint8_t> rf_address_;
};


} // namespace itho_ecofanrft
} // namespace esphome