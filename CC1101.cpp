#include "esphome/core/log.h"
#include "cc1101.h"
#include "cc1101_reg.h"


namespace esphome {
namespace itho_ecofanrft {

static const char *TAG = "itho_ecofanrft.cc1101";

bool CC1101::init() {
    uint8_t partnum, version;

    this->reset_();

    partnum = read_register_(CC1101_PARTNUM);
    version = read_register_(CC1101_VERSION);

    if (version == 0x00 || version == 0xFF) {
        ESP_LOGE(TAG, "No CC1101 found");
        return false;
    }

    this->write_command_strobe(CC1101_SFRX);
    delayMicroseconds(100);
    this->write_command_strobe(CC1101_SFTX);
    delayMicroseconds(100);

    ESP_LOGCONFIG(TAG, "Found CC1101: partnum (%x), version (%x)", partnum, version);
    return true;
}

void CC1101::send_data(const std::vector<uint8_t> &data) {

    uint8_t marcstate = 0xFF;

    this->flush_txfifo();
    this->send();

    this->write_burst_register(CC1101_TXFIFO, data);

    while (marcstate != CC1101_MARCSTATE_IDLE && marcstate != CC1101_MARCSTATE_TXFIFO_UNDERFLOW) {
        yield();
        marcstate = (this->read_register(CC1101_MARCSTATE) & CC1101_BITS_MARCSTATE);
    }
    ESP_LOGV(TAG, "CC1101 done sending: MARCSTATE (%x)", marcstate);
}

std::vector<uint8_t> CC1101::receive_data(const uint8_t max_length) {
    std::vector <uint8_t> in_bytes;
    uint8_t in_bytes_available = this->read_register(CC1101_RXBYTES) & CC1101_BITS_BYTES_IN_FIFO;

    in_bytes  = this->read_burst_register(CC1101_RXFIFO, in_bytes_available);

    return in_bytes;
}

uint8_t CC1101::write_command_strobe(const uint8_t command) {
    uint8_t status_byte;

    this->select_();
    status_byte = this->spi_->transfer_byte(command | CC1101_WRITE_SINGLE);
    this->deselect_();

    return status_byte;
}

uint8_t CC1101::sidle() {
    uint8_t marcstate = 0xFF;

    ESP_LOGVV(TAG, "CC1101 strobe SIDLE");

    this->write_command_strobe(CC1101_SIDLE);

    while (marcstate != CC1101_MARCSTATE_IDLE) {
        yield();
        ESP_LOGVV(TAG, "CC1101 check MARCSTATE for IDLE");
        marcstate = (this->read_register(CC1101_MARCSTATE) & CC1101_BITS_MARCSTATE);
    }
    return marcstate;
}

void CC1101::flush_rxfifo() {
    this->sidle();
    this->write_command_strobe(CC1101_SFRX);
    delayMicroseconds(100);
}

void CC1101::flush_txfifo() {
    this->sidle();
    this->write_command_strobe(CC1101_SFTX);
    delayMicroseconds(100);
}

uint8_t CC1101::receive() {
    uint8_t marcstate = 0xFF;
    uint8_t counter = 0;

    ESP_LOGVV(TAG, "CC1101 receive");

    //this->sidle();
    //this->write_command_strobe(CC1101_SFRX);
    //delayMicroseconds(100);

    this->sidle();
    this->write_command_strobe(CC1101_SRX);

    while (marcstate != CC1101_MARCSTATE_RX) {
        yield();
        marcstate = (this->read_register(CC1101_MARCSTATE) & CC1101_BITS_MARCSTATE);

        ESP_LOGVV(TAG, "CC1101 check MARCSTATE for RX (current state %02X)", marcstate);

        if (marcstate == CC1101_MARCSTATE_RXFIFO_OVERFLOW) {
            this->write_command_strobe(CC1101_SFRX);
        }

        if (++counter > 200) {
            ESP_LOGW(TAG, "CC1101 stuck, retry SRX strobe");
            this->sidle();
            this->write_command_strobe(CC1101_SFRX);
            delayMicroseconds(100);
            this->write_command_strobe(CC1101_SRX);
            counter = 0;
        }

    }
    return marcstate;
}

uint8_t CC1101::send() {
    uint8_t marcstate = 0xFF;
    uint8_t counter = 0;

    ESP_LOGVV(TAG, "CC1101 send");

    //this->sidle();
    //this->write_command_strobe(CC1101_SFRX);
    //delayMicroseconds(100);

    this->sidle();
    this->write_command_strobe(CC1101_STX);

    while (marcstate != CC1101_MARCSTATE_TX) {
        yield();
        marcstate = (this->read_register(CC1101_MARCSTATE) & CC1101_BITS_MARCSTATE);

        ESP_LOGVV(TAG, "CC1101 check MARCSTATE for TX (current state %02X)", marcstate);

        if (++counter > 200) {
            ESP_LOGW(TAG, "CC1101 stuck, retry STX strobe");
            this->sidle();
            this->write_command_strobe(CC1101_SFTX);
            delayMicroseconds(100);
            this->write_command_strobe(CC1101_STX);
            counter = 0;
        }

    }
    return marcstate;
}



void CC1101::write_register(const uint8_t address, const uint8_t data) {
    this->select_();
    this->spi_->write_byte(address | CC1101_WRITE_SINGLE);
    this->spi_->write_byte(data);
    this->deselect_();
}

uint8_t CC1101::read_register(const uint8_t address) {

    switch (address) {
        case CC1101_FREQEST:
        case CC1101_MARCSTATE:
        case CC1101_RXBYTES:
        case CC1101_TXBYTES:
        case CC1101_WORTIME0:
        case CC1101_WORTIME1:
              return this->read_register_with_sync_problem_(address);
    }
    return this->read_register_(address);
}

void CC1101::write_burst_register(const uint8_t address, const std::vector<uint8_t> &data) {

    if (address >= 0x00 && address <= 0x2E) {
        // Normal config registers
        if (address + data.size() > 0x2F) {
            ESP_LOGW(TAG, "Burst write requested beyond last register");
        }
    } else if (address == 0x3E) {
        // PA table registers
        if (data.size() > 8) {
            ESP_LOGW(TAG, "Burst write wraps counter in PATABLE register");
        }
    } else if (address != 0x3F) {
        ESP_LOGE(TAG, "Invalid register for burst write (%x)", address);
        return;
    }

    this->select_();
    this->spi_->write_byte(address | CC1101_WRITE_BURST);
    for (uint8_t out_byte : data) {
        this->spi_->write_byte(out_byte);
    }
    this->deselect_();

}

std::vector<uint8_t> CC1101::read_burst_register(const uint8_t address, const uint8_t max_length) {
    uint8_t count, i;
    std::vector <uint8_t> in_bytes;

    if (address >= 0x00 && address <= 0x2E) {
        // Normal config registers
        if (address + max_length > 0x2F) {
            ESP_LOGW(TAG, "Burst read requested beyond last register");
            count = 0x2F - address;
        } else {
            count = max_length;
        }
    } else if (address == 0x3E) {
        // PA table registers
        count = min(max_length, (uint8_t) 8);
    } else if (address == 0x3F) {
        // FIFO data
        count = max_length;
    } else {
        ESP_LOGE(TAG, "Invalid register for burst read (%x)", address);
        return in_bytes;
    }

    this->select_();
    this->spi_->write_byte(address | CC1101_READ_BURST);
    for (i = 0; i < count; i++) {
        in_bytes.push_back(this->spi_->transfer_byte(0x00));
    }
    this->deselect_();

    return in_bytes;
}

uint8_t CC1101::read_register_(const uint8_t address) {
    uint8_t in_byte;

    this->select_();
    if (address >= 0x30 && address <= 0x3D) {
      this->spi_->write_byte(address | CC1101_READ_BURST);
    } else {
      this->spi_->write_byte(address | CC1101_READ_SINGLE);
    }
    in_byte = this->spi_->transfer_byte(0x00);
    this->deselect_();

    return in_byte;
}

uint8_t CC1101::read_register_with_sync_problem_(const uint8_t address) {
    uint8_t in_byte1, in_byte2;

    in_byte1 = this->read_register_(address);

    do {
        in_byte2 = in_byte1;
        in_byte1 = this->read_register_(address);
    } while (in_byte1 != in_byte2);

    return in_byte1;
}

void CC1101::reset_(bool power_on_reset) {

  if (power_on_reset) {
    this->cs_->digital_write(true);
    delayMicroseconds(10);
    this->cs_->digital_write(false);
    delayMicroseconds(10);
    this->cs_->digital_write(true);
    delayMicroseconds(40);
  }

  this->select_();
  this->spi_->write_byte(CC1101_SRES);
  this->deselect_();

  delay(1);

  // In IDLE state
}


int16_t CC1101::read_rssi() {
    uint8_t raw_rssi = this->read_register(CC1101_RSSI);
    int16_t rssi;
    if (raw_rssi >= 128) {
        rssi = (raw_rssi - 256)/2 - 74;
    } else {
        rssi = (raw_rssi)/2 - 74;
    }
    return rssi;
}


} // namespace itho_ecofanrft
} // namespace esphome