/**
 * @file nh3_sensor.h
 * @brief Internal logic for communicating with ammonia
 * click nh3 sensor using spi
 *
 * @copyright Copyright (c) 2023 OAMK Corosect-project.
 *
 */
#ifndef BE821905_0997_4832_BD03_42B95D20393B
#define BE821905_0997_4832_BD03_42B95D20393B

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CS_PIN 27
#define SCK_PIN 26
#define SDO_PIN 2

/**
 * @brief Data structure to hold nh3 spi data in a easy to use format.
 */
struct nh3_spi_data {
  bool OL;
  bool OH;
  uint32_t value;
};

/**
 * @brief Initializes spi and sets the required configs
 * for communicating with the nh3 sensor.
 */
void init_nh3();

/**
 * @brief Performs an read on the sensor and prints the result to serail.
 *
 */
void read_and_print_nh3();

/**
 * @brief Tries to read nh3 data from the spi
 *
 * @return 0 on success and a negative error code on error
 */
int read_nh3_data();

/**
 * @brief Helper to print the spi data array.
 *
 * @param spi_data byte array
 * @param size size of the array
 */
void print_spi_data(uint8_t *spi_data, size_t size);

/**
 * @brief Formats an byte array to `struct nh3_spi_data` format.
 * Requires at least three bytes and all byte after three are ignored.
 *
 * @param spi_data byte array
 * @param size size of the array
 * @param _out_data pointer to the output variable
 *
 * @return nh3 data in `_out_data` pointer
 *
 */
void format_nh3_data(
    uint8_t *spi_data, size_t size, struct nh3_spi_data *_out_data
);

/**
 * @brief Prints `nh3_spi_data` struct to serial
 *
 * @param data pointer to the data struct to print
 */
void print_nh3_struct(struct nh3_spi_data *data);

#endif /* BE821905_0997_4832_BD03_42B95D20393B */
