#include "driver/i2c.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "font8x8_basic.h"

#define SDA_PIN 4
#define SCL_PIN 5
#define MASTER_PORT 0
#define OLED_FREQ 100000
#define OLED_ADDRESS 0x3C
#define WRITE_BIT 0
#define READ_BIT 1
#define ACK_EN 1

#define TAG "OLED"

#define OLED_CMD_SET_HORI_ADDR_MODE     0x00    
#define OLED_CMD_SET_VERT_ADDR_MODE     0x01    
#define OLED_CMD_SET_PAGE_ADDR_MODE     0x02    
#define OLED_CMD_SET_COLUMN_RANGE       0x21    


esp_err_t i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    esp_err_t err = i2c_param_config(MASTER_PORT, &conf);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(MASTER_PORT, conf.mode, 0, 0, 0);
}

void ssd1306_init() {
    esp_err_t espRc;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);

    i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP, true);
    i2c_master_write_byte(cmd, 0x14, true);

    i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP, true); // reverse left-right mapping
    i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE, true); // reverse up-bottom mapping

    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_NORMAL, true);
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_OFF, true);
    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);
    i2c_master_stop(cmd);

    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (espRc == ESP_OK) {
        ESP_LOGI(TAG, "OLED configured successfully");
    }
    else {
        ESP_LOGE(TAG, "OLED configuration failed. code: 0x%.2X", espRc);
    }
    i2c_cmd_link_delete(cmd);
}

void write_text(int page, int column, char* data, int len) {
    int cur_page = page;
    int cur_col = column;
    if (cur_page > 7) 
        return;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, OLED_ADDRESS << 1 | WRITE_BIT, ACK_EN);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, ACK_EN);
    
    i2c_master_write_byte(cmd, OLED_CMD_SET_MEMORY_ADDR_MODE, ACK_EN);
    i2c_master_write_byte(cmd, OLED_CMD_SET_PAGE_ADDR_MODE, ACK_EN);

    i2c_master_write_byte(cmd, 0x0F & column , ACK_EN); //lower byte for start column
    i2c_master_write_byte(cmd, 0x10 | column >> 4 , ACK_EN); //higher byte for start column

    i2c_master_write_byte(cmd, 0xB0 + page, ACK_EN);
    
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(MASTER_PORT, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    
    for (int i = 0; i < len; i++) {
        if (cur_col > 120) {
            cur_col = 0;
            cur_page++;
            if (cur_page > 7)
                return;
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, OLED_ADDRESS << 1 | WRITE_BIT, ACK_EN);
            i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, ACK_EN);

            i2c_master_write_byte(cmd, 0x0F & cur_col , ACK_EN); //lower byte for start column
            i2c_master_write_byte(cmd, 0x10 | cur_col >> 4 , ACK_EN);

            i2c_master_write_byte(cmd, 0xB0 + cur_page, ACK_EN);

            i2c_master_stop(cmd);
            i2c_master_cmd_begin(MASTER_PORT, cmd, 1000 / portTICK_RATE_MS);
            i2c_cmd_link_delete(cmd);
        }
        
        if (data[i] == '\n') {
            cur_col = 127;
            continue;
        }

        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, OLED_ADDRESS << 1 | WRITE_BIT, ACK_EN);

        i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, ACK_EN);   

        i2c_master_write(cmd, font8x8_basic_tr[(uint8_t)data[i]], 8, ACK_EN);

        i2c_master_stop(cmd);
        i2c_master_cmd_begin(MASTER_PORT, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
        cur_col += 8;
    }
}

void task_ssd1306_display_clear() {
	i2c_cmd_handle_t cmd;

	uint8_t clear[128];
	for (uint8_t i = 0; i < 128; i++) {
		clear[i] = 0;
	}
	for (uint8_t i = 0; i < 8; i++) {
		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_SINGLE, true);
		i2c_master_write_byte(cmd, 0xB0 | i, true);

		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
		i2c_master_write(cmd, clear, 128, true);
		i2c_master_stop(cmd);
		i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);
	}
}

void print_image(uint8_t  arg_text[][128]) {

    i2c_cmd_handle_t cmd;
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
	i2c_master_write_byte(cmd, OLED_CMD_SET_MEMORY_ADDR_MODE, true); // reset column - choose column --> 0
	i2c_master_write_byte(cmd, OLED_CMD_SET_HORI_ADDR_MODE, true); // reset line - choose line --> 0
	i2c_master_write_byte(cmd, OLED_CMD_SET_PAGE_RANGE, true); // reset page
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write_byte(cmd, 0x07, true);

    i2c_master_write_byte(cmd, OLED_CMD_SET_COLUMN_RANGE, true); // reset page
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write_byte(cmd, 0x7f, true);

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
    for(int i = 0; i < 8; i++){
        for(int j=0; j < 128; j++){
            i2c_master_write_byte(cmd, arg_text[i][j], true);
        }
    }
    i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 2300/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
}
uint8_t image[8][128] = {{0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000},
{0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b10000000,0b11000000,0b11100000,0b11100000,0b11110000,0b11111000,0b01111000,0b10111100,0b11111100,0b01011100,0b10111110,0b11101110,0b01011110,0b00110110,0b00101110,0b00011010,0b10010110,0b10010100,0b11001100,0b11001010,0b11100110,0b11100100,0b01100100,0b01110000,0b01110000,0b00110000,0b00110000,0b00111000,0b00111000,0b01011000,0b10011000,0b00011000,0b00011100,0b00011100,0b00001100,0b00111100,0b01111110,0b01111111,0b01111111,0b01111111,0b01111110,0b00111100,0b00110000,0b01110000,0b11100000,0b11000000,0b10000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000},
{0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b11100000,0b10111000,0b01011100,0b10101111,0b01010111,0b00101011,0b00111011,0b10010101,0b11001010,0b11000101,0b11100101,0b11110010,0b01110001,0b00111001,0b00111100,0b00011100,0b00001110,0b00001110,0b00000111,0b00000111,0b00000011,0b00000011,0b00000001,0b00000001,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000001,0b00000110,0b00011000,0b01100000,0b10000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000001,0b11111111,0b00111100,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000},
{0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b10001000,0b11000101,0b11100010,0b11110001,0b11111000,0b01111100,0b00011110,0b00001111,0b00000111,0b00000011,0b00000001,0b00000001,0b11100000,0b11111000,0b01111100,0b10111110,0b11011111,0b11101110,0b11110110,0b11111000,0b11111100,0b11111000,0b11110010,0b11100110,0b11001111,0b10011111,0b01111110,0b11111000,0b11100000,0b10000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000111,0b00110000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00100000,0b00001100,0b00000011,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000},
{0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b11000000,0b11110000,0b11111000,0b11111110,0b11111111,0b00111111,0b00001111,0b00000111,0b00000001,0b00000000,0b00000000,0b00001110,0b00111110,0b00111110,0b10111110,0b10000000,0b10001110,0b10100010,0b10111100,0b10111110,0b00111101,0b00011101,0b00001101,0b00001011,0b00000011,0b00000011,0b00000011,0b00001011,0b00001101,0b00011101,0b00111101,0b10111110,0b10111100,0b10110000,0b10001110,0b10000000,0b10111110,0b00111110,0b00111110,0b00011110,0b00000000,0b00000000,0b00000000,0b10000000,0b01110000,0b00001111,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000},
{0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b11110000,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11100011,0b00000000,0b00000000,0b00000000,0b00000000,0b00000110,0b00001110,0b00011110,0b00111110,0b01111110,0b01111100,0b11111001,0b11110001,0b11110011,0b11100111,0b11100111,0b11001111,0b11011110,0b11111100,0b11110000,0b11100000,0b11000000,0b11100000,0b11110000,0b11111000,0b11011110,0b11001111,0b11101111,0b11100111,0b11100011,0b11110001,0b11111001,0b01111000,0b01111100,0b00111110,0b00011110,0b00001110,0b00000110,0b00000011,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000},
{0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000111,0b00001111,0b00011111,0b00111111,0b01111111,0b01111111,0b01111111,0b11111110,0b11111100,0b11111000,0b11111000,0b11111000,0b11110000,0b11110000,0b11110000,0b11110000,0b11110000,0b11110000,0b01110001,0b01110001,0b01110001,0b01110011,0b00110011,0b00110011,0b00110011,0b00011011,0b00011011,0b00011011,0b00001111,0b00001111,0b00000111,0b00000111,0b00000011,0b00000011,0b00000001,0b00000001,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000},
{0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000,0b00000000}};

void app_main() {
    i2c_master_init();
    ssd1306_init();
    task_ssd1306_display_clear();
    write_text(1, 32, "20521784", 8);
    write_text(3, 32, "20520788", 8);
    write_text(5, 32, "20522141", 8);
}

