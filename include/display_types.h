/**
 * @file display_types.h
 * @brief Display data structures
 */

#ifndef DISPLAY_TYPES_H
#define DISPLAY_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float temp[4];
    float humidity[4];
    bool valid[4];
    uint8_t num_sensors;
} display_sensor_data_t;

#endif