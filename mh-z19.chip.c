// Wokwi Custom Chip - NDIR CO2 Sensor
// Simulates an NDIR Sensor with Analog (A0) and Digital Alarm (D0) outputs

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  pin_t pin_a0;       // Analog voltage output pin
  pin_t pin_d0;       // Digital alarm output pin
  uint32_t ppm_attr;  // Attribute handle for slider control
} chip_data_t;

#define MAX_PPM 5000.0f
#define ALARM_THRESHOLD_PPM 3000 // Digital D0 triggers HIGH at 3000+ PPM

void chip_timer_callback(void *user_data) {
  chip_data_t *chip_data = (chip_data_t *)user_data;

  // Read CO2 concentration set by the UI slider
  uint32_t ppm = attr_read(chip_data->ppm_attr);

  // Linear scaling: 0 to 5000 PPM -> 0.0V to 3.3V (ESP32 ADC compatible)
  float volts = ((float)ppm / MAX_PPM) * 5.f;
  
  if (volts > 5.f) volts = 5.f;
  if (volts < 0.0f) volts = 0.0f;

  // Write analog voltage output
  pin_dac_write(chip_data->pin_a0, volts);

  // Write digital alarm state (HIGH if PPM >= threshold)
  if (ppm >= ALARM_THRESHOLD_PPM) {
    pin_write(chip_data->pin_d0, HIGH);
  } else {
    pin_write(chip_data->pin_d0, LOW);
  }
}

void chip_init() {
  chip_data_t *chip_data = (chip_data_t *)malloc(sizeof(chip_data_t));

  // Initialize slider attribute with default baseline ambient air (400 PPM)
  chip_data->ppm_attr = attr_init("ppm", 400);

  // Initialize output pins
  chip_data->pin_a0 = pin_init("A0", ANALOG);
  chip_data->pin_d0 = pin_init("D0", OUTPUT);

  const timer_config_t config = {
    .callback = chip_timer_callback,
    .user_data = chip_data,
  };

  // Run callback every 20ms (20,000 µs)
  timer_t timer_id = timer_init(&config);
  timer_start(timer_id, 20000, true);
}