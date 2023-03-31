#ifndef BE821905_0997_4832_BD03_42B95D20393B
#define BE821905_0997_4832_BD03_42B95D20393B

#include <nrfx_spim.h>

#define CS_PIN 27
#define SCK_PIN 26
#define SDO_PIN 2

struct nh3_spi_data {
  bool OL;
  bool OH;
  uint32_t value;
};

void init_nh3();
void read_and_print_nh3();

int read_nh3_data(nrfx_spim_t *instance, nrfx_spim_xfer_desc_t *transf);

void print_spi_data(uint8_t *spi_data, size_t size);
void format_nh3_data(uint8_t *spi_data, struct nh3_spi_data *_out_data);
void print_nh3_struct(struct nh3_spi_data *data);

#endif /* BE821905_0997_4832_BD03_42B95D20393B */
