#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <string>

namespace esphome {
namespace iec61107 {

// Kamstrup 382 IEC 61107 mode A readout.
//
// On each poll the component sends the readout request "/?!\r\n", buffers the
// response until [ETX] + BCC has been received (or a timeout elapses),
// validates the block check character, and publishes the OBIS fields.
class IEC61107Component : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_response_timeout(uint32_t timeout_ms) { this->timeout_ms_ = timeout_ms; }

  void set_type_number_sensor(text_sensor::TextSensor *s) { this->type_number_sensor_ = s; }
  void set_serial_number_sensor(text_sensor::TextSensor *s) { this->serial_number_sensor_ = s; }
  void set_energy_sensor(sensor::Sensor *s) { this->energy_sensor_ = s; }
  void set_tariff1_sensor(sensor::Sensor *s) { this->tariff1_sensor_ = s; }
  void set_tariff2_sensor(sensor::Sensor *s) { this->tariff2_sensor_ = s; }
  void set_hour_counter_sensor(sensor::Sensor *s) { this->hour_counter_sensor_ = s; }
  void set_pulse_input_sensor(sensor::Sensor *s) { this->pulse_input_sensor_ = s; }
  void set_peak_power_sensor(sensor::Sensor *s) { this->peak_power_sensor_ = s; }
  void set_last_month_peak_sensor(sensor::Sensor *s) { this->last_month_peak_sensor_ = s; }

 protected:
  enum class State { IDLE, WAITING };

  void send_request_();
  void process_frame_();
  bool parse_line_(const std::string &line);
  void publish_number_(sensor::Sensor *s, const std::string &value);

  State state_{State::IDLE};
  std::string buffer_;
  uint32_t request_time_{0};
  uint32_t timeout_ms_{2000};

  text_sensor::TextSensor *type_number_sensor_{nullptr};
  text_sensor::TextSensor *serial_number_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};
  sensor::Sensor *tariff1_sensor_{nullptr};
  sensor::Sensor *tariff2_sensor_{nullptr};
  sensor::Sensor *hour_counter_sensor_{nullptr};
  sensor::Sensor *pulse_input_sensor_{nullptr};
  sensor::Sensor *peak_power_sensor_{nullptr};
  sensor::Sensor *last_month_peak_sensor_{nullptr};
};

}  // namespace iec61107
}  // namespace esphome
