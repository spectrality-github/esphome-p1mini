//-------------------------------------------------------------------------------------
// ESPHome P1 Electricity Meter custom sensor
// Copyright 2022 Johnny Johansson
// Copyright 2020 Pär Svanström
// 
// History
//  0.1.0 2020-11-05:   Initial release
//  0.2.0 2022-04-13:   Major rewrite
//
// MIT License
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
// IN THE SOFTWARE.
//-------------------------------------------------------------------------------------

#include "esphome.h"

class P1Reader : public Component, public UARTDevice {
public:

  // Call from a lambda in the yaml file to set up each sensor.
  Sensor *AddSensor(int major, int minor, int micro)
  {
    m_sensor_list = new SensorListItem(m_sensor_list, OBIS(major, minor, micro));
    return m_sensor_list->GetSensor();
  }

  P1Reader(unsigned int minimum_period_ms,
           UARTComponent *parent,
           esphome::gpio::GPIOSwitch * CTS_switch,
           esphome::gpio::GPIOSwitch * status_switch = nullptr)
    : UARTDevice(parent) 
    , m_minimum_period_ms{ minimum_period_ms }
    , m_CTS_switch{ CTS_switch }
    , m_status_switch{ status_switch }
    {}

private:
  unsigned long const m_minimum_period_ms{ 0 };
  unsigned long m_period_start_time;

  // Store the main message as it is beeing received:
  constexpr static int message_buffer_size{ 2048 };
  char message_buffer[message_buffer_size];
  int message_buffer_position{ 0 };

  // Store the CRC part of the message as it is beeing received:
  constexpr static int crc_buffer_size{ 8 };
  char crc_buffer[crc_buffer_size];
  int crc_buffer_position{ 0 };

  // Calculate the CRC while the message is beeing received
  uint16_t crc{ 0 };

  enum class states {
    READING_MESSAGE,
    READING_CRC,
    PROCESSING,
    WAITING
  };
  enum states m_state{ states::READING_MESSAGE };

  void ChangeState(enum states new_state)
  {
    switch (new_state) {
    case states::READING_MESSAGE:
      m_CTS_switch->turn_on();
      if (m_status_switch != nullptr) m_status_switch->turn_on();
      crc = message_buffer_position = 0;
      break;
    case states::READING_CRC:
      crc_buffer_position = 0;
      break;
    case states::PROCESSING:
      m_CTS_switch->turn_off();
      break;
    case states::WAITING:
      if (m_state != states::PROCESSING) m_CTS_switch->turn_off();
      if (m_status_switch != nullptr) m_status_switch->turn_off();
      break;
    }
    m_state = new_state;
  }

  // Combine the three values defining a sensor into a single unsigned int for easier
  // handling and comparison
  static uint32_t OBIS(uint32_t major, uint32_t minor, uint32_t micro)
  {
    return (major & 0xfff) << 16 | (minor & 0xff)  << 8 | (micro & 0xff);
  }

  class SensorListItem {
    uint32_t m_obisCode;
    Sensor m_sensor;
    SensorListItem *m_next{ nullptr };
  public:
    SensorListItem(SensorListItem *next, uint32_t obisCode)
    : m_obisCode(obisCode)
    , m_next(next)
    {}
    
    Sensor *GetSensor() { return &m_sensor; }
    uint32_t GetCode() { return m_obisCode; }
    SensorListItem *Next() { return m_next; }
  };

  // Linked list of all sensors
  SensorListItem *m_sensor_list { nullptr };

  esphome::gpio::GPIOSwitch * const m_CTS_switch;
  esphome::gpio::GPIOSwitch * const m_status_switch;

public:

  void setup() override 
  {
    m_period_start_time = millis();
    ChangeState(states::READING_MESSAGE);
  }

  void loop() override {
    switch (m_state) {
    case states::READING_MESSAGE:
    case states::READING_CRC:
      while (available()) {
        // While data is available, read it one byte at a time.
        char const read_byte{ (char)read() }; 
        if (m_state == states::READING_MESSAGE) {
          if (message_buffer_position == message_buffer_size) {
            ESP_LOGW("p1reader", "Message buffer overrun. Resetting.");
            ChangeState(states::WAITING);
            while (available()) read();
            return;
          }
          crc16_update(read_byte);
          if (read_byte == '!') {
            // The exclamation mark indicates that the main message is complete
            // and the CRC will come next.
            message_buffer[message_buffer_position] = '\0';
            ChangeState(states::READING_CRC);
          }
          else message_buffer[message_buffer_position++] = read_byte;
        }
        else { // READING_CRC
          if (crc_buffer_position == crc_buffer_size) {
            ESP_LOGW("p1reader", "CRC buffer overrun. Resetting.");
            ChangeState(states::WAITING);
            while (available()) read();
            return;
          }
          if (read_byte == '\n') {
            // The CRC is a single line, so once we reach end of line, we are
            // ready to verify and process the message.
            crc_buffer[crc_buffer_position] = '\0';
            int const crcFromMsg = (int) strtol(crc_buffer, NULL, 16);
            if (crc != crcFromMsg) {
              ESP_LOGW("p1reader", "CRC missmatch, calculated %04X != %04X. Message ignored.", crc, crcFromMsg);
              ESP_LOGD("p1reader", "Buffer:\n%s", message_buffer);
              ChangeState(states::WAITING);
              return;
            }
            else {
              ChangeState(states::PROCESSING);
              return;
            }
          }
          else {
            crc_buffer[crc_buffer_position++] = read_byte;
          }
        }
      }
      break;
    case states::PROCESSING:
      ParseMessage();
      ChangeState(states::WAITING);
      break;
    case states::WAITING:
      unsigned long const current_time{ millis() };
      if (m_minimum_period_ms < current_time - m_period_start_time) {
        m_period_start_time = current_time;
        ChangeState(states::READING_MESSAGE);
      }
      break;
    }
  }

private:
  void crc16_update(uint8_t a) {
    int i;
    crc ^= a;
    for (i = 0; i < 8; ++i) {
      if (crc & 1) {
          crc = (crc >> 1) ^ 0xA001;
      } else {
          crc = (crc >> 1);
      }
    }
  }


  // Find the matching sensor in the linked list (or return nullptr
  // if it does not exist.
  Sensor *GetSensor(uint32_t obisCode) const
  {
    SensorListItem *sensor_list { m_sensor_list };
    while (sensor_list != nullptr) {
      if (obisCode == sensor_list->GetCode()) return sensor_list->GetSensor();
      sensor_list = sensor_list->Next();
    }
    return nullptr;
  }

  // Once a complete message has been received and CRC-verified, this method
  // will process the contents.
  void ParseMessage() {
    char *start_of_line{ message_buffer };
    while (*start_of_line == '\n' || *start_of_line == '\r') ++start_of_line;
    
    // Message will be processed one line at a time:
    while (*start_of_line != '\0') {
      char *end_of_line{ start_of_line };
      while (*end_of_line != '\n' && *end_of_line != '\r' && *end_of_line != '\0') ++end_of_line;
      bool const last_line{ *end_of_line == '\0' };
      *end_of_line = '\0';

      if (end_of_line != start_of_line) {
        int minor{ -1 }, major{ -1 }, micro{ -1 };
        double value{ -1.0 };
        if (sscanf(start_of_line, "1-0:%d.%d.%d(%lf", &major, &minor, &micro, &value) != 4) {
          ESP_LOGD("p1reader", "Could not parse value from line '%s'", start_of_line);
        }
        else {
          uint32_t const obisCode{ OBIS(major, minor, micro) };
          Sensor *S{ GetSensor(obisCode) };
          if (S != nullptr) S->publish_state(value);
          else {
            ESP_LOGD("p1reader", "No sensor matching: %d.%d.%d (0x%x)", major, minor, micro, obisCode);
          }
        }
      }
      if (last_line) return;
      start_of_line = end_of_line + 1;
      while (*start_of_line == '\n' || *start_of_line == '\r') ++start_of_line;
    }
  }


};
