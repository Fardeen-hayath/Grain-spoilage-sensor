// Wokwi Custom Chip - Soil Moisture Sensor

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  pin_t pin;
  uint32_t moisture_attr; // Using uint32_t instead of attr_t
} chip_data_t;

void chip_timer_callback(void *user_data) {
  chip_data_t *chip_data = (chip_data_t *)user_data;
  
  uint32_t moisture = attr_read(chip_data->moisture_attr);
  
  // Convert 0-1023 analog range to 0-3.3V output for ESP32
  float volts = 3.3f * ((float)moisture / 1023.0f);
  
  pin_dac_write(chip_data->pin, volts);
}

void chip_init() {
  chip_data_t *chip_data = (chip_data_t *)malloc(sizeof(chip_data_t));
  
  chip_data->moisture_attr = attr_init("moisture", 512);
  chip_data->pin = pin_init("SIG", ANALOG);

  const timer_config_t config = {
    .callback = chip_timer_callback,
    .user_data = chip_data,
  };

  timer_t timer_id = timer_init(&config);
  timer_start(timer_id, 10000, true); // Triggers every 10ms
}