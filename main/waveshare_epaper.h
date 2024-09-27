#pragma once

#include "driver/spi_master.h"

#define EPD_SCK_PIN 12
#define EPD_MOSI_PIN 11
#define EPD_CS_PIN 13
#define EPD_RST_PIN 15
#define EPD_DC_PIN 14
#define EPD_BUSY_PIN 16
#define EPD_PWR_PIN 17

class GPIOPin {
    int pin_;
    gpio_mode_t mode_;
    bool inverted_;

public:
    GPIOPin(int pin, gpio_mode_t mode, bool inverted = false) : pin_(pin), mode_(mode), inverted_(inverted) {}

    bool digital_read();
    void digital_write(bool value);
    void setup();
};

class Display {
public:
    virtual void update() = 0;
    virtual void setup() = 0;
    virtual void on_safe_shutdown() = 0;
    virtual void dump_config() = 0;
    void set_cs_pin(GPIOPin *cs_pin) { cs_pin_ = cs_pin; }
    uint8_t *get_buffer() { return buffer_; }
    int get_width() { return get_width_internal(); }
    int get_height() { return get_height_internal(); }

protected:
    void delay(int ms);
    uint32_t millis();
    void init_internal_(size_t buffer_length);
    void do_update_();
    void spi_setup();
    void enable();
    void disable();
    void write_byte(uint8_t value);
    void write_array(const uint8_t *data, size_t len);

    virtual int get_height_internal() = 0;
    virtual int get_width_internal() = 0;

    uint8_t *buffer_{nullptr};
    GPIOPin *cs_pin_{nullptr};
    spi_device_handle_t spi_{nullptr};
};

class WaveshareEPaperBase : public Display {
public:
    void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
    void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
    void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }
    void set_reset_duration(uint32_t reset_duration) { this->reset_duration_ = reset_duration; }

    void command(uint8_t value);
    void data(uint8_t value);
    void cmd_data(const uint8_t *data, size_t length);

    virtual void display() = 0;
    virtual void initialize() = 0;
    virtual void deep_sleep() = 0;

    void update() override;

    void setup() override {
        this->setup_pins_();
        this->initialize();
    }

    void on_safe_shutdown() override;

protected:
    bool wait_until_idle_();

    void setup_pins_();

    void reset_() {
        if (this->reset_pin_ != nullptr) {
            this->reset_pin_->digital_write(false);
            delay(reset_duration_);  // NOLINT
            this->reset_pin_->digital_write(true);
            delay(20);
        }
    }

    virtual int get_width_controller() { return this->get_width_internal(); };

    virtual uint32_t get_buffer_length_() = 0;  // NOLINT(readability-identifier-naming)
    uint32_t reset_duration_{200};

    void start_command_();
    void end_command_();
    void start_data_();
    void end_data_();

    GPIOPin *reset_pin_{nullptr};
    GPIOPin *dc_pin_;
    GPIOPin *busy_pin_{nullptr};
    virtual uint32_t idle_timeout_() { return 120'000u; }  // NOLINT(readability-identifier-naming)
};

class WaveshareEPaper : public WaveshareEPaperBase {
protected:
    uint32_t get_buffer_length_() override;
};

class WaveshareEPaper7P5InV2 : public WaveshareEPaper {
public:
    bool wait_until_idle_();

    void initialize() override;

    void display() override;

    void dump_config() override;

    void deep_sleep() override {
        // COMMAND POWER OFF
        this->command(0x02);
        this->wait_until_idle_();
        // COMMAND DEEP SLEEP
        this->command(0x07);
        this->data(0xA5);  // check byte
    }

    void set_full_update_every(uint32_t full_update_every) { full_update_every_ = full_update_every; }

protected:
    int get_width_internal() override;

    int get_height_internal() override;

    uint32_t idle_timeout_() override;

    uint32_t full_update_every_{30};
    uint32_t at_update_{0};
};

class WaveshareEPaper7P5InV2alt : public WaveshareEPaper7P5InV2 {
public:
    bool wait_until_idle_();
    void initialize() override;
    void dump_config() override;

protected:
    void reset_() {
        if (this->reset_pin_ != nullptr) {
            this->reset_pin_->digital_write(true);
            delay(200);  // NOLINT
            this->reset_pin_->digital_write(false);
            delay(2);
            this->reset_pin_->digital_write(true);
            delay(20);
        }
    };
};
