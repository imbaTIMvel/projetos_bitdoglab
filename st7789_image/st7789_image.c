#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "image_data.h"

// ==== Pin definitions ====
#define PIN_MOSI 19
#define PIN_SCK  18
#define PIN_CS   17
#define PIN_DC    4
#define PIN_RST  20
#define PIN_BL    9

#define ST7789_WIDTH  320
#define ST7789_HEIGHT 240

// ==== ST7789 Commands ====
#define ST7789_CASET  0x2A
#define ST7789_RASET  0x2B
#define ST7789_RAMWR  0x2C
#define ST7789_MADCTL 0x36
#define ST7789_COLMOD 0x3A
#define ST7789_SLPOUT 0x11
#define ST7789_DISPON 0x29

// ==== Helper functions ====
static inline void st7789_select()   { gpio_put(PIN_CS, 0); }
static inline void st7789_deselect() { gpio_put(PIN_CS, 1); }
static inline void st7789_dc_cmd()   { gpio_put(PIN_DC, 0); }
static inline void st7789_dc_data()  { gpio_put(PIN_DC, 1); }

void st7789_write_cmd(uint8_t cmd) {
    st7789_select();
    st7789_dc_cmd();
    spi_write_blocking(spi0, &cmd, 1);
    st7789_deselect();
}

void st7789_write_data(const uint8_t *data, size_t len) {
    st7789_select();
    st7789_dc_data();
    spi_write_blocking(spi0, data, len);
    st7789_deselect();
}

void st7789_write_data_byte(uint8_t d) {
    st7789_write_data(&d, 1);
}

// ==== ST7789 Functions ====
void st7789_init() {
    // Init GPIO
    gpio_init(PIN_CS);  gpio_set_dir(PIN_CS, GPIO_OUT);  gpio_put(PIN_CS, 1);
    gpio_init(PIN_DC);  gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_init(PIN_BL);  gpio_set_dir(PIN_BL, GPIO_OUT);

    // Init SPI
    spi_init(spi0, 40000000); // 40 MHz
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);

    // Reset display
    gpio_put(PIN_RST, 0);
    sleep_ms(50);
    gpio_put(PIN_RST, 1);
    sleep_ms(50);

    // Backlight on
    gpio_put(PIN_BL, 1);

    // Sleep Out
    st7789_write_cmd(ST7789_SLPOUT);
    sleep_ms(120);

    // Color Mode: 16-bit
    st7789_write_cmd(ST7789_COLMOD);
    st7789_write_data_byte(0x55);

    // Memory Access Control (Rotation 0)
    st7789_write_cmd(ST7789_MADCTL);
    st7789_write_data_byte(0x00);

    // Display On
    st7789_write_cmd(ST7789_DISPON);
    sleep_ms(120);
}

void st7789_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    st7789_write_cmd(ST7789_RASET);
    data[0] = x0 >> 8; data[1] = x0 & 0xFF;
    data[2] = x1 >> 8; data[3] = x1 & 0xFF;
    st7789_write_data(data, 4);

    st7789_write_cmd(ST7789_CASET);
    data[0] = y0 >> 8; data[1] = y0 & 0xFF;
    data[2] = y1 >> 8; data[3] = y1 & 0xFF;
    st7789_write_data(data, 4);

    st7789_write_cmd(ST7789_RAMWR);
}

void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT)) return;
    if ((x + w - 1) >= ST7789_WIDTH) w = ST7789_WIDTH - x;
    if ((y + h - 1) >= ST7789_HEIGHT) h = ST7789_HEIGHT - y;

    st7789_set_addr_window(x, y, x + w - 1, y + h - 1);

    uint8_t data[2] = { color >> 8, color & 0xFF };
    st7789_select();
    st7789_dc_data();
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        spi_write_blocking(spi0, data, 2);
    }
    st7789_deselect();
}

void st7789_fill_screen(uint16_t color) {
    st7789_fill_rect(0, 0, ST7789_WIDTH, ST7789_HEIGHT, color);
}

void st7789_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data) {
    if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT)) return;
    if ((x + w - 1) >= ST7789_WIDTH)  w = ST7789_WIDTH - x;
    if ((y + h - 1) >= ST7789_HEIGHT) h = ST7789_HEIGHT - y;

    st7789_set_addr_window(x, y, x + w - 1, y + h - 1);

    st7789_select();
    st7789_dc_data();
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        uint8_t full_data[2] = { data[i * 2], data[(i * 2) + 1] };
        spi_write_blocking(spi0, full_data, 2);
    }
    st7789_deselect();
}

#define RGB565(r,g,b) (~(((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F)) & 0xFFFF)

// ==== Main example ====
int main() {
    stdio_init_all();
    st7789_init();

    // Fill black
    st7789_fill_screen(RGB565(0,0,0));
    sleep_ms(100);

    // Draw single block
    st7789_fill_rect(110,140,100,40,RGB565(31,0,0));    // RED
    st7789_fill_rect(110,100,100,40,RGB565(0,63,0));    // GRN
    st7789_fill_rect(110,60,100,40,RGB565(0,0,31));     // BLU
    sleep_ms(200);
    st7789_fill_screen(RGB565(0,0,0));
    sleep_ms(100);

    st7789_draw_bitmap(0, 0, IMG_WIDTH, IMG_HEIGHT, image_data);

    while (1) tight_loop_contents();
}
