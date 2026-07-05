#include "iec61107.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cstdlib>

namespace esphome {
namespace iec61107 {

static const char *const TAG = "iec61107";

static const char STX = 0x02;  // start of text
static const char ETX = 0x03;  // end of text
static const char CR = 0x0D;
static const char LF = 0x0A;

static const size_t MAX_FRAME = 512;

void IEC61107Component::setup() {
  // Drop any noise the UART may have captured during boot.
  while (this->available())
    this->read();
}

void IEC61107Component::update() {
  if (this->state_ != State::IDLE) {
    ESP_LOGW(TAG, "Previous readout still in progress, skipping this poll");
    return;
  }
  this->send_request_();
}

void IEC61107Component::send_request_() {
  // Flush the RX buffer, then send the mode A readout request.
  while (this->available())
    this->read();
  this->buffer_.clear();

  this->write_str("/?!\r\n");
  this->flush();

  this->request_time_ = millis();
  this->state_ = State::WAITING;
  ESP_LOGD(TAG, "Sent readout request '/?!'");
}

void IEC61107Component::loop() {
  if (this->state_ != State::WAITING)
    return;

  while (this->available()) {
    uint8_t c;
    if (!this->read_byte(&c))
      break;
    this->buffer_.push_back(static_cast<char>(c));

    if (this->buffer_.size() > MAX_FRAME) {
      ESP_LOGW(TAG, "Response exceeded %d bytes without [ETX], discarding", (int) MAX_FRAME);
      this->state_ = State::IDLE;
      return;
    }
  }

  // A frame is complete once we have received [ETX] plus the trailing BCC byte.
  size_t etx_pos = this->buffer_.find(ETX);
  if (etx_pos != std::string::npos && this->buffer_.size() >= etx_pos + 2) {
    this->process_frame_();
    this->state_ = State::IDLE;
    return;
  }

  if (millis() - this->request_time_ > this->timeout_ms_) {
    ESP_LOGW(TAG, "Timeout waiting for meter response (%d bytes received)", (int) this->buffer_.size());
    this->state_ = State::IDLE;
  }
}

void IEC61107Component::process_frame_() {
  size_t stx_pos = this->buffer_.find(STX);
  size_t etx_pos = this->buffer_.find(ETX);
  if (stx_pos == std::string::npos || etx_pos == std::string::npos || etx_pos < stx_pos) {
    ESP_LOGW(TAG, "Malformed frame (missing STX/ETX)");
    return;
  }

  const uint8_t received_bcc = static_cast<uint8_t>(this->buffer_[etx_pos + 1]) & 0x7F;

  // BCC = 7-bit addition of every character after [STX] up to and including [ETX].
  uint8_t sum = 0;
  for (size_t i = stx_pos + 1; i <= etx_pos; i++)
    sum += static_cast<uint8_t>(this->buffer_[i]);
  sum &= 0x7F;

  if (sum != received_bcc) {
    ESP_LOGW(TAG, "BCC mismatch: computed 0x%02X, received 0x%02X - dropping frame", sum, received_bcc);
    return;
  }
  ESP_LOGD(TAG, "BCC OK (0x%02X)", sum);

  // Identification line, e.g. "/KAM 685-382-OK-15", sits before [STX].
  if (this->type_number_sensor_ != nullptr) {
    size_t start = this->buffer_.find('/');
    if (start != std::string::npos && start < stx_pos) {
      size_t end = this->buffer_.find(CR, start);
      if (end == std::string::npos || end > stx_pos)
        end = stx_pos;
      std::string header = this->buffer_.substr(start, end - start);
      this->type_number_sensor_->publish_state(header);
    }
  }

  // Data records live between [STX] and [ETX], one OBIS field per CRLF line.
  size_t pos = stx_pos + 1;
  while (pos < etx_pos) {
    size_t eol = this->buffer_.find(CR, pos);
    if (eol == std::string::npos || eol > etx_pos)
      eol = etx_pos;

    std::string line = this->buffer_.substr(pos, eol - pos);
    while (!line.empty() && (line.back() == CR || line.back() == LF))
      line.pop_back();
    while (!line.empty() && (line.front() == CR || line.front() == LF))
      line.erase(line.begin());

    if (!line.empty())
      this->parse_line_(line);

    pos = eol + 1;
    if (pos < this->buffer_.size() && this->buffer_[pos] == LF)
      pos++;
  }
}

bool IEC61107Component::parse_line_(const std::string &line) {
  size_t lp = line.find('(');
  size_t rp = line.find(')');
  if (lp == std::string::npos || rp == std::string::npos || rp < lp)
    return false;

  std::string code = line.substr(0, lp);
  std::string inside = line.substr(lp + 1, rp - lp - 1);

  // Values are formatted "value" or "value*unit"; keep only the numeric part.
  std::string value = inside;
  size_t star = inside.find('*');
  if (star != std::string::npos)
    value = inside.substr(0, star);

  // Kamstrup uses a decimal comma (e.g. "000018,3").
  for (auto &ch : value)
    if (ch == ',')
      ch = '.';

  ESP_LOGV(TAG, "OBIS %s = '%s'", code.c_str(), value.c_str());

  if (code == "0.0") {
    if (this->serial_number_sensor_ != nullptr)
      this->serial_number_sensor_->publish_state(value);
  } else if (code == "1.20") {
    this->publish_number_(this->energy_sensor_, value);
  } else if (code == "1.20.1") {
    this->publish_number_(this->tariff1_sensor_, value);
  } else if (code == "1.20.2") {
    this->publish_number_(this->tariff2_sensor_, value);
  } else if (code == "1.31") {
    this->publish_number_(this->hour_counter_sensor_, value);
  } else if (code == "1.26") {
    this->publish_number_(this->pulse_input_sensor_, value);
  } else if (code == "1.6") {
    this->publish_number_(this->peak_power_sensor_, value);
  } else if (code == "1.6*1") {
    this->publish_number_(this->last_month_peak_sensor_, value);
  } else {
    return false;
  }
  return true;
}

void IEC61107Component::publish_number_(sensor::Sensor *s, const std::string &value) {
  if (s == nullptr)
    return;
  char *end = nullptr;
  float v = strtof(value.c_str(), &end);
  if (end == value.c_str()) {
    ESP_LOGW(TAG, "Could not parse numeric value '%s'", value.c_str());
    return;
  }
  s->publish_state(v);
}

void IEC61107Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Kamstrup 382 IEC 61107:");
  ESP_LOGCONFIG(TAG, "  Response timeout: %u ms", (unsigned) this->timeout_ms_);
  this->check_uart_settings(300, 1, uart::UART_CONFIG_PARITY_EVEN, 7);
}

}  // namespace iec61107
}  // namespace esphome
