/*
 * Platform services used above the HAL: clock, delay, tasks, large buffers and
 * the critical section. core/, gfx/, ui/ and apps/ call these instead of
 * ESP-IDF or FreeRTOS, so those layers can be built for a host target by
 * supplying another implementation of this header.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace hal::sys {

// Monotonic microseconds since boot.
int64_t nowUs();

// Blocks the calling task, yielding the CPU.
void delayMs(int ms);

// Starts a task pinned to `core`. Returns false if it could not be created.
using TaskFn = void (*)(void* arg);
bool spawnPinned(TaskFn fn, const char* name, uint32_t stack_bytes, void* arg,
                 int priority, int core);

// Ends the calling task (a task started by spawnPinned must not return).
[[noreturn]] void endTask();

// Reboots the chip.
[[noreturn]] void restart();

// Byte-addressable buffer in PSRAM. Returns nullptr on failure.
void* allocLarge(size_t bytes);
// Byte-addressable buffer in internal RAM. Returns nullptr on failure.
void* allocInternal(size_t bytes);
// Releases a buffer from allocLarge() or allocInternal().
void  freeBuffer(void* p);

// Firmware version embedded at build time (`git describe`, e.g. "v0.1.0").
const char* firmwareVersion();

size_t freeHeapBytes();   // all heaps
size_t freePsramBytes();  // PSRAM only

// Process-wide critical section for very short state updates shared between
// tasks. Do not block or nest inside it.
void criticalEnter();
void criticalExit();

}  // namespace hal::sys
