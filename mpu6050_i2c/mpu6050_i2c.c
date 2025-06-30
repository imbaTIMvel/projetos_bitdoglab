#include <stdio.h>
#include <string.h>
#include <stdlib.h>             // For display SSD1306
#include <ctype.h>              // For display SSD1306
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"        // For display SSD1306
#include "hardware/i2c.h"

// GP0 is SDA for I2C0
// GP1 is SCL for I2C0
#define i2c0_sda 0
#define i2c0_scl 1

// I2C for display SSD1306
#define i2c_sda 14
#define i2c_scl 15

// By default these devices  are on bus address 0x68
static int addr = 0x68;

#ifdef i2c_default
static void mpu6050_reset() {
    // Two byte reset. First byte register, second byte data
    // There are a load more options to set up the device in different ways that could be added here
    uint8_t buf[] = {0x6B, 0x80};
    i2c_write_blocking(i2c_default, addr, buf, 2, false);
    sleep_ms(100); // Allow device to reset and stabilize

    // Clear sleep mode (0x6B register, 0x00 value)
    buf[1] = 0x00;  // Clear sleep mode by writing 0x00 to the 0x6B register
    i2c_write_blocking(i2c_default, addr, buf, 2, false); 
    sleep_ms(10); // Allow stabilization after waking up
}

static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    // For this particular device, we send the device the register we want to read
    // first, then subsequently read from the device. The register is auto incrementing
    // so we don't need to keep sending the register we want, just the first.

    uint8_t buffer[6];

    // Start reading acceleration registers from register 0x3B for 6 bytes
    uint8_t val = 0x3B;
    i2c_write_blocking(i2c_default, addr, &val, 1, true); // true to keep master control of bus
    i2c_read_blocking(i2c_default, addr, buffer, 6, false);

    for (int i = 0; i < 3; i++) {
        accel[i] = (buffer[i * 2] << 8 | buffer[(i * 2) + 1]);
    }

    // Now gyro data from reg 0x43 for 6 bytes
    // The register is auto incrementing on each read
    val = 0x43;
    i2c_write_blocking(i2c_default, addr, &val, 1, true);
    i2c_read_blocking(i2c_default, addr, buffer, 6, false);  // False - finished with bus

    for (int i = 0; i < 3; i++) {
        gyro[i] = (buffer[i * 2] << 8 | buffer[(i * 2) + 1]);;
    }

    // Now temperature from reg 0x41 for 2 bytes
    // The register is auto incrementing on each read
    val = 0x41;
    i2c_write_blocking(i2c_default, addr, &val, 1, true);
    i2c_read_blocking(i2c_default, addr, buffer, 2, false);  // False - finished with bus

    *temp = buffer[0] << 8 | buffer[1];
}
#endif

int main() {
    stdio_init_all();
#if !defined(i2c_default) || !defined(i2c0_sda) || !defined(i2c0_scl)
    #warning i2c/mpu6050_i2c example requires a board with I2C pins
    puts("Default I2C pins were not defined");
    return 0;
#else
    printf("Hello, MPU6050! Reading raw data from registers...\n");

    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(i2c0_sda, GPIO_FUNC_I2C);
    gpio_set_function(i2c0_scl, GPIO_FUNC_I2C);
    gpio_pull_up(i2c0_sda);
    gpio_pull_up(i2c0_scl);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(i2c0_sda, i2c0_scl, GPIO_FUNC_I2C));

    mpu6050_reset();

    int16_t acceleration[3], gyro[3], temp;

    // Configurar display
    // Inicialização do i2c
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(i2c_sda, GPIO_FUNC_I2C);
    gpio_set_function(i2c_scl, GPIO_FUNC_I2C);
    gpio_pull_up(i2c_sda);
    gpio_pull_up(i2c_scl);

    // Inicialização do display SSD1306
    ssd1306_init();

    // Preparar área de renderização para o display
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    // Zerar o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    while (1) {
        mpu6050_read_raw(acceleration, gyro, &temp);

        /*
        // These are the raw numbers from the chip, so will need tweaking to be really useful.
        // See the datasheet for more information
        printf("Acc. X = %d, Y = %d, Z = %d\n", acceleration[0], acceleration[1], acceleration[2]);
        printf("Gyro. X = %d, Y = %d, Z = %d\n", gyro[0], gyro[1], gyro[2]);
        // Temperature is simple so use the datasheet calculation to get deg C.
        // Note this is chip temperature.
        printf("Temp. = %f\n", (temp / 340.0) + 36.53);
        */

        char buffer[6][20]; // Buffer para 6 strings de até 20 caracteres

        snprintf(buffer[0], sizeof(buffer[0]), "AX = %d", (int)acceleration[0]);
        snprintf(buffer[1], sizeof(buffer[1]), "AY = %d", (int)acceleration[1]);
        snprintf(buffer[2], sizeof(buffer[2]), "AZ = %d", (int)acceleration[2]);
        snprintf(buffer[3], sizeof(buffer[3]), "GX = %d", (int)gyro[0]);
        snprintf(buffer[4], sizeof(buffer[4]), "GY = %d", (int)gyro[1]);
        snprintf(buffer[5], sizeof(buffer[5]), "GZ = %d", (int)gyro[2]);

        int y = 0;
        for (uint i = 0; i < count_of(buffer); i++)
        {
            ssd1306_draw_string(ssd, 5, y, buffer[i]);
            y += 8;
        }
        render_on_display(ssd, &frame_area);

        sleep_ms(400);

        // Zerar o display inteiro
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        render_on_display(ssd, &frame_area);
    }
#endif
}
