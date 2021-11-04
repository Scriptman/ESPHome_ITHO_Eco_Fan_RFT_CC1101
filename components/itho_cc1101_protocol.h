#pragma once

/**
 * Based on code from:
 *  Klusjesman: https://github.com/Klusjesman/IthoEcoFanRFT
 *  supersjimmie: https://github.com/supersjimmie/IthoEcoFanRFT
 *  Wim Philipsen: https://github.com/philipsen/IthoRadio
 *
 * Author: Jasper van der Neut
 * Changes:
 *   - Validated CC1101 parameters with SmartRF tool
 *   - Use CC1101 hardware for preamble and sync word
 *   - Use packet size smaller than FIFO, more reliable
 *   - Adapted for Esphome
 */

#include <unordered_map>
#include "cc1101_reg.h"

namespace esphome {
namespace itho_ecofanrft {

static const uint8_t MAX_PACKET_LEN = 0x3C;     // 60 bytes max in the air. Less than max RX fifo size.

using register_setting = struct {
    uint8_t address;
    uint8_t data;
};

/**
 * Configuration reverse engineered from remote print.
 *
 * Base frequency          868.299866 MHz
 * Channel                 0
 * Channel spacing         199.951172 kHz
 * Carrier frequency       868.299866 MHz
 * Xtal frequency          26.000000 MHz
 * Data rate               38.3835 kBaud
 * Manchester              disabled
 * Modulation              2-FSK
 * Deviation               50.781250 kHz
 * TX power                10 dBm
 * PA ramping              enabled
 * Whitening               disabled
 */
static const std::vector<register_setting> ITHO_CC1101_CONFIG
{
  {CC1101_IOCFG2,      0x01},           // Assert when RX FIFO is above threshold or at end of packet
  {CC1101_IOCFG0,      0x2E},           // High-Z
  {CC1101_FIFOTHR,     0x4E},           // ADC retention, 0dB RX attenuation, 5/60 FIFO threshold
  {CC1101_SYNC1,       0xAB},           // RX sync word, MSB
  {CC1101_SYNC0,       0xFE},           // RX sync word, LSB
  {CC1101_PKTLEN,      MAX_PACKET_LEN}, // Packet length
  {CC1101_PKTCTRL1,    0xA0},           // PQT 10*4 ~ 5 bytes of preamble
  {CC1101_PKTCTRL0,    0x00},           // Fixed packet length mode
  {CC1101_FSCTRL1,     0x06},           // SmartRF
  {CC1101_FREQ2,       0x21},           // SmartRF
  {CC1101_FREQ1,       0x65},           // SmartRF
  {CC1101_FREQ0,       0x6A},           // SmartRF
  {CC1101_MDMCFG4,     0x9A},           // SmartRF
  {CC1101_MDMCFG3,     0x83},           // SmartRF
  {CC1101_MDMCFG2,     0x06},           // 2-FSK, 16 bit sync word with carrier-sense
  {CC1101_MDMCFG1,     0x42},           // 8 bytes preamble
  {CC1101_DEVIATN,     0x50},           // SmartRF
  {CC1101_MCSM0,       0x18},           // Manual calibration
  {CC1101_FOCCFG,      0x16},           // SmartRF
  {CC1101_AGCCTRL2,    0x43},           // SmartRF
  {CC1101_AGCCTRL1,    0x49},           // SmartRF
  {CC1101_WORCTRL,     0xFB},           // SmartRF
  {CC1101_FREND0,      0x17},           // Use full PATABLE power settings
  {CC1101_FSCAL3,      0xE9},           // SmartRF
  {CC1101_FSCAL2,      0x2A},           // SmartRF
  {CC1101_FSCAL1,      0x00},           // SmartRF
  {CC1101_FSCAL0,      0x1F},           // SmartRF
  {CC1101_TEST2,       0x81},           // SmartRF
  {CC1101_TEST1,       0x35},           // SmartRF
  {CC1101_TEST0,       0x09},           // SmartRF
};

/**
 * SmartRF preset for 10 dBm. Note some of the original values are invalid,
 * or should not be used.
 */
static const std::vector<uint8_t> ITHO_CC1101_PATABLE = {0x00,0x03,0x0F,0x27,0x50,0xC8,0xC3,0xC5};

/**
 * Itho packet consists of the following:
 *  - header
 *  - payload: command or status (manchester encoded)
 *  - footer / terminator
 */
namespace packet {
    // header
    static const std::vector<uint8_t> ITHO_CC1101_HEADER = {0x00, 0xb3, 0x2a, 0xab, 0x2a};

    // payload
    static const std::vector<uint8_t> INDICATOR_STATUS = { 0x14 };
    static const std::vector<uint8_t> INDICATOR_COMMAND = { 0x16 };
    // 0x18
    // 0x1C

    // footer
    static const uint8_t FOOTER_EVEN = 0xac;
    static const uint8_t FOOTER_ODD = 0xca;
    static const std::vector<uint8_t> ITHO_CC1101_FOOTER = {FOOTER_EVEN, FOOTER_ODD};
    static const uint8_t POSTAMBLE = 0xaa;
} // namespace packet

/**
 * Command messages
 *
 * These commands should be equivalent to a basic RFT remote (536-0123) and/or
 * RFT-AUTO remote (536-0150) and should work with the following models:
 *  - CVE(-S)
 *  - CVD(-S)
 *  - HRU ECO 200/300/350
 */
static const std::unordered_map<std::string, std::vector<uint8_t>> ITHO_COMMANDS = {
    { "min",    { 0x22, 0xf1, 0x03, 0x00, 0x01, 0x04 } },
    { "low",    { 0x22, 0xf1, 0x03, 0x00, 0x02, 0x04 } },
    { "medium", { 0x22, 0xf1, 0x03, 0x00, 0x03, 0x04 } },   // auto with sensors
    { "high",   { 0x22, 0xf1, 0x03, 0x00, 0x04, 0x04 } },
    { "max",    { 0x22, 0xf1, 0x03, 0x00, 0x05, 0x04 } },

    { "timer1", { 0x22, 0xf3, 0x03, 0x00, 0x80, 0x01 } },
    { "timer2", { 0x22, 0xf3, 0x03, 0x00, 0x80, 0x02 } },
    { "timer3", { 0x22, 0xf3, 0x03, 0x00, 0x80, 0x03 } },

    { "join",   { 0x1f, 0xc9, 0x0c, 0x00, 0x22, 0xf1 } },   // ... <join> <id> <join_2> <id> <checksum>
    { "join_2", { 0x01, 0x10, 0xe0 } },
    { "leave",  { 0x1f, 0xc9, 0x06, 0x00, 0x1f, 0xc9 } },   // ... <leave> <id> <checksum>
};

/**
 * Status messages:
 *  lead_in: 1 byte (0x14)
 *  id: 3 bytes (e.g. 0x50 0x72 0xA2)
 *  msg: 5 bytes
 *  value: 1 byte
 *  checksum: 1 byte
 */
static const std::vector<uint8_t> ITHO_STATUS = { 0x31, 0xd9, 0x03, 0x00, 0x00 };

static const std::vector<uint8_t> ITHO_KEEPALIVE = { 0x1f, 0xd4, 0x03, 0x00 }; // +2 bytes

};  // namespace itho_ecofanrft
} // namespace esphome