#include "includes.h"

#include "MotionSensor.h"

constexpr auto SENSOR_GPIO = GPIO_NUM_6;

MotionSensor::MotionSensor(Queue* synchronizationQueue) : _synchronizationQueue(synchronizationQueue) {}

void MotionSensor::begin() {
    _queue = xQueueCreate(10, sizeof(gpio_num_t));

    gpio_config_t config{
        .pin_bit_mask = (1ull << SENSOR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };

    gpio_config(&config);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(
        SENSOR_GPIO, [](auto arg) { ((MotionSensor*)arg)->interruptHandler(); }, this);

    xTaskCreate([](auto arg) { ((MotionSensor*)arg)->taskHandler(); }, "gpio_interrupt_task", 2048, this, 10, nullptr);
}

void MotionSensor::taskHandler() {
    while (true) {
        gpio_num_t gpio;
        xQueueReceive(_queue, &gpio, portMAX_DELAY);
    }
}

void MotionSensor::interruptHandler() { _triggered.queue(_synchronizationQueue); }
