/* Stub: lets the firmware's mixer and synth compile natively (tests/render_music.c). */
#ifndef STUB_FREERTOS_H
#define STUB_FREERTOS_H
typedef void *SemaphoreHandle_t;
typedef void *TaskHandle_t;
#define pdMS_TO_TICKS(ms) (ms)
static inline void vTaskDelay(unsigned ticks) { (void)ticks; }
#endif
