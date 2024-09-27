// Adapted from
// https://github.com/esphome/esphome/blob/release/esphome/components/waveshare_epaper/waveshare_epaper.cpp.

#include "includes.h"

#include "waveshare_epaper.h"

#include "support.h"

LOG_TAG(WaveshareEPaper);

#define HOT __attribute__((hot))

#define LOG_DISPLAY(...)
#define LOG_UPDATE_INTERVAL(...)
#define LOG_PIN(...)
#define ESP_LOGCONFIG(...)

bool GPIOPin::digital_read() { return gpio_get_level((gpio_num_t)pin_) == 0 ? inverted_ : !inverted_; }

void GPIOPin::digital_write(bool value) { gpio_set_level((gpio_num_t)pin_, value ? !inverted_ : inverted_); }

void GPIOPin::setup() {
    gpio_config_t i_conf = {
        .pin_bit_mask = 1ull << pin_,
        .mode = mode_,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&i_conf));
}

void Display::delay(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

uint32_t Display::millis() { return esp_timer_get_time() / 1000; }

void Display::init_internal_(size_t buffer_length) {
    buffer_ = (uint8_t *)heap_caps_malloc(buffer_length, MALLOC_CAP_SPIRAM);
    ESP_ERROR_CHECK(buffer_ ? ESP_OK : ESP_ERR_NO_MEM);
}

void Display::do_update_() {}

void Display::spi_setup() {
    this->cs_pin_->setup();

    spi_bus_config_t bus_config = {
        .mosi_io_num = EPD_MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = EPD_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t device_interface_config = {
        .clock_speed_hz = 2'000'000,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &device_interface_config, &spi_));
}

void Display::enable() { this->cs_pin_->digital_write(false); }

void Display::disable() { this->cs_pin_->digital_write(true); }

void Display::write_byte(uint8_t value) {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data = {value},
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi_, &t));
}

void Display::write_array(const uint8_t *data, size_t len) {
    spi_transaction_t t = {
        .length = 8 * len,
        .tx_buffer = data,
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi_, &t));
}

void WaveshareEPaperBase::setup_pins_() {
    this->init_internal_(this->get_buffer_length_());
    this->dc_pin_->setup();  // OUTPUT
    this->dc_pin_->digital_write(false);
    if (this->reset_pin_ != nullptr) {
        this->reset_pin_->setup();  // OUTPUT
        this->reset_pin_->digital_write(true);
    }
    if (this->busy_pin_ != nullptr) {
        this->busy_pin_->setup();  // INPUT
    }
    this->spi_setup();

    this->reset_();
}

void WaveshareEPaperBase::command(uint8_t value) {
    this->start_command_();
    this->write_byte(value);
    this->end_command_();
}

void WaveshareEPaperBase::data(uint8_t value) {
    this->start_data_();
    this->write_byte(value);
    this->end_data_();
}

// write a command followed by one or more bytes of data.
// The command is the first byte, length is the total including cmd.
void WaveshareEPaperBase::cmd_data(const uint8_t *c_data, size_t length) {
    this->dc_pin_->digital_write(false);
    this->enable();
    this->write_byte(c_data[0]);
    this->dc_pin_->digital_write(true);
    this->write_array(c_data + 1, length - 1);
    this->disable();
}

bool WaveshareEPaperBase::wait_until_idle_() {
    if (this->busy_pin_ == nullptr || !this->busy_pin_->digital_read()) {
        return true;
    }

    const uint32_t start = millis();
    while (this->busy_pin_->digital_read()) {
        if (millis() - start > this->idle_timeout_()) {
            ESP_LOGE(TAG, "Timeout while displaying image!");
            return false;
        }
        delay(20);
    }
    return true;
}

void WaveshareEPaperBase::update() {
    this->do_update_();
    this->display();
}

void WaveshareEPaperBase::start_command_() {
    this->dc_pin_->digital_write(false);
    this->enable();
}

void WaveshareEPaperBase::end_command_() { this->disable(); }

void WaveshareEPaperBase::start_data_() {
    this->dc_pin_->digital_write(true);
    this->enable();
}

void WaveshareEPaperBase::end_data_() { this->disable(); }

void WaveshareEPaperBase::on_safe_shutdown() { this->deep_sleep(); }

uint32_t WaveshareEPaper::get_buffer_length_() {
    return this->get_width_controller() * this->get_height_internal() / 8u;
}  // just a black buffer

bool WaveshareEPaper7P5InV2::wait_until_idle_() {
    if (this->busy_pin_ == nullptr) {
        return true;
    }

    const uint32_t start = millis();
    while (this->busy_pin_->digital_read()) {
        this->command(0x71);
        if (millis() - start > this->idle_timeout_()) {
            ESP_LOGE(TAG, "Timeout while displaying image!");
            return false;
        }
        delay(20);
    }
    return true;
}
void WaveshareEPaper7P5InV2::initialize() {
    ESP_LOGI(TAG, "Initializing display");

    // COMMAND POWER SETTING
    this->command(0x01);
    this->data(0x07);
    this->data(0x07);
    this->data(0x3f);
    this->data(0x3f);

    // We don't want the display to be powered at this point

    delay(100);  // NOLINT
    this->wait_until_idle_();

    // COMMAND VCOM AND DATA INTERVAL SETTING
    this->command(0x50);
    this->data(0x10);
    this->data(0x07);

    // COMMAND TCON SETTING
    this->command(0x60);
    this->data(0x22);

    // COMMAND PANEL SETTING
    this->command(0x00);
    this->data(0x1F);

    // COMMAND RESOLUTION SETTING
    this->command(0x61);
    this->data(0x03);
    this->data(0x20);
    this->data(0x01);
    this->data(0xE0);

    // COMMAND DUAL SPI MM_EN, DUSPI_EN
    this->command(0x15);
    this->data(0x00);

    // COMMAND POWER DRIVER HAT DOWN
    // This command will turn off booster, controller, source driver, gate driver, VCOM, and
    // temperature sensor, but register data will be kept until VDD turned OFF or Deep Sleep Mode.
    // Source/Gate/Border/VCOM will be released to floating.
    this->command(0x02);
}
void HOT WaveshareEPaper7P5InV2::display() {
    bool partial = this->at_update_ != 0;
    this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;

    uint32_t buf_len = this->get_buffer_length_();

    // COMMAND POWER ON
    ESP_LOGI(TAG, "Power on the display and hat");

    // This command will turn on booster, controller, regulators, and temperature sensor will be
    // activated for one-time sensing before enabling booster. When all voltages are ready, the
    // BUSY_N signal will return to high.
    this->command(0x04);
    delay(200);  // NOLINT
    this->wait_until_idle_();

    if (partial) {
        this->command(0x91);  // Partial in
        this->command(0x90);  // Partial Window
        this->data(0x00);     // x-start
        this->data(0x00);
        this->data(0x00);  // y-start
        this->data(0x00);
        this->data(0x03);  // x-end
        this->data(0x20 - 1);
        this->data(0x01);  // y-end
        this->data(0xE0 - 1);
        this->data(0x01);  // Gate scan selection

        // Update the partial area
        this->command(0x24);
    } else {
        // COMMAND DATA START TRANSMISSION NEW DATA
        this->command(0x13);
    }

    delay(2);
    for (uint32_t i = 0; i < buf_len; i++) {
        this->data(~(this->buffer_[i]));
    }

    delay(100);  // NOLINT
    this->wait_until_idle_();

    // COMMAND DISPLAY REFRESH
    this->command(0x12);
    delay(100);  // NOLINT
    this->wait_until_idle_();

    if (partial) {
        this->command(0x92);  // Partial out
    }

    ESP_LOGV(TAG, "Before command(0x02) (>> power off)");
    this->command(0x02);
    this->wait_until_idle_();
    ESP_LOGV(TAG, "After command(0x02) (>> power off)");
}

int WaveshareEPaper7P5InV2::get_width_internal() { return 800; }

int WaveshareEPaper7P5InV2::get_height_internal() { return 480; }

uint32_t WaveshareEPaper7P5InV2::idle_timeout_() { return 10000; }

void WaveshareEPaper7P5InV2::dump_config() {
    LOG_DISPLAY("", "Waveshare E-Paper", this);
    ESP_LOGCONFIG(TAG, "  Model: 7.5inV2rev2");
    LOG_PIN("  Reset Pin: ", this->reset_pin_);
    LOG_PIN("  DC Pin: ", this->dc_pin_);
    LOG_PIN("  Busy Pin: ", this->busy_pin_);
    LOG_UPDATE_INTERVAL(this);
}

/* 7.50inV2alt */
bool WaveshareEPaper7P5InV2alt::wait_until_idle_() {
    if (this->busy_pin_ == nullptr) {
        return true;
    }

    const uint32_t start = millis();
    while (this->busy_pin_->digital_read()) {
        this->command(0x71);
        if (millis() - start > this->idle_timeout_()) {
            ESP_LOGI(TAG, "Timeout while displaying image!");
            return false;
        }
        delay(20);
    }
    return true;
}

void WaveshareEPaper7P5InV2alt::initialize() {
    this->reset_();

    // COMMAND POWER SETTING
    this->command(0x01);

    // 1-0=11: internal power
    this->data(0x07);
    this->data(0x17);  // VGH&VGL
    this->data(0x3F);  // VSH
    this->data(0x26);  // VSL
    this->data(0x11);  // VSHR

    // VCOM DC Setting
    this->command(0x82);
    this->data(0x24);  // VCOM

    // Booster Setting
    this->command(0x06);
    this->data(0x27);
    this->data(0x27);
    this->data(0x2F);
    this->data(0x17);

    // POWER ON
    this->command(0x04);

    delay(100);  // NOLINT
    this->wait_until_idle_();
    // COMMAND PANEL SETTING
    this->command(0x00);
    this->data(0x3F);  // KW-3f   KWR-2F BWROTP 0f BWOTP 1f

    // COMMAND RESOLUTION SETTING
    this->command(0x61);
    this->data(0x03);  // source 800
    this->data(0x20);
    this->data(0x01);  // gate 480
    this->data(0xE0);
    // COMMAND ...?
    this->command(0x15);
    this->data(0x00);
    // COMMAND VCOM AND DATA INTERVAL SETTING
    this->command(0x50);
    this->data(0x10);
    this->data(0x00);
    // COMMAND TCON SETTING
    this->command(0x60);
    this->data(0x22);
    // Resolution setting
    this->command(0x65);
    this->data(0x00);
    this->data(0x00);  // 800*480
    this->data(0x00);
    this->data(0x00);

    this->wait_until_idle_();

    uint8_t lut_vcom_7_i_n5_v2[] = {
        0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0xF, 0x1, 0xF, 0x1, 0x2, 0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    };

    uint8_t lut_ww_7_i_n5_v2[] = {
        0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
        0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    };

    uint8_t lut_bw_7_i_n5_v2[] = {
        0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
        0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    };

    uint8_t lut_wb_7_i_n5_v2[] = {
        0x80, 0xF, 0xF, 0x0, 0x0, 0x3, 0x84, 0xF, 0x1, 0xF, 0x1, 0x4, 0x40, 0xF, 0xF, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0,
        0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    };

    uint8_t lut_bb_7_i_n5_v2[] = {
        0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
        0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    };

    uint8_t count;
    this->command(0x20);  // VCOM
    for (count = 0; count < 42; count++) this->data(lut_vcom_7_i_n5_v2[count]);

    this->command(0x21);  // LUTBW
    for (count = 0; count < 42; count++) this->data(lut_ww_7_i_n5_v2[count]);

    this->command(0x22);  // LUTBW
    for (count = 0; count < 42; count++) this->data(lut_bw_7_i_n5_v2[count]);

    this->command(0x23);  // LUTWB
    for (count = 0; count < 42; count++) this->data(lut_wb_7_i_n5_v2[count]);

    this->command(0x24);  // LUTBB
    for (count = 0; count < 42; count++) this->data(lut_bb_7_i_n5_v2[count]);
}

void WaveshareEPaper7P5InV2alt::dump_config() {
    LOG_DISPLAY("", "Waveshare E-Paper", this);
    ESP_LOGCONFIG(TAG, "  Model: 7.5inV2");
    LOG_PIN("  Reset Pin: ", this->reset_pin_);
    LOG_PIN("  DC Pin: ", this->dc_pin_);
    LOG_PIN("  Busy Pin: ", this->busy_pin_);
    LOG_UPDATE_INTERVAL(this);
}
