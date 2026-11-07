#include "hal_sys.h"

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace hal::sys {
namespace {
portMUX_TYPE s_critical = portMUX_INITIALIZER_UNLOCKED;
}  // namespace

int64_t nowUs() { return esp_timer_get_time(); }

void delayMs(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

bool spawnPinned(TaskFn fn, const char* name, uint32_t stack_bytes, void* arg,
                 int priority, int core)
{
    return xTaskCreatePinnedToCore(fn, name, stack_bytes, arg, priority, nullptr,
                                   core) == pdPASS;
}

void* allocLarge(size_t bytes)
{
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void* allocInternal(size_t bytes)
{
    return heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
}

void freeBuffer(void* p) { heap_caps_free(p); }

size_t freeHeapBytes() { return esp_get_free_heap_size(); }

size_t freePsramBytes() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }

void criticalEnter() { portENTER_CRITICAL(&s_critical); }

void criticalExit() { portEXIT_CRITICAL(&s_critical); }

}  // namespace hal::sys
