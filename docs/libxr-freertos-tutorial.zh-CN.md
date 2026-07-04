# LibXR FreeRTOS 使用教程

本文面向当前 `MSPM0G3507_LibXR_Template` 工程，说明如何把模板从
`LIBXR_SYSTEM None` 切换到 FreeRTOS，并说明 LibXR 在 FreeRTOS 后端下的
线程、信号量、互斥锁、队列、定时器和异步任务怎么用。

这不是 LibXR 公共 API 的修改说明。文中的代码片段用于指导你迁移工程，当前仓库
默认仍是无 RTOS 模式。

## 当前工程是什么状态

当前模板是单线程裸机结构：

- `cmake/LibXR.CMake` 中 `LIBXR_SYSTEM` 还是 `None`。
- 根 `CMakeLists.txt` 给应用目标定义了 `LIBXR_NOT_SUPPORT_MUTI_THREAD=1`。
- `src/main.c` 调用 `SYSCFG_DL_init()` 后手动初始化 SysTick，然后直接调用
  `app_main()`。
- `src/app_main.cpp` 在 `app_main()` 里创建 `MSPM0Timebase`、GPIO、UART，
  然后进入永久 `while (true)` 循环。

这个结构可以使用 `LibXR::Thread::Sleep()`，但在 `LIBXR_SYSTEM None` 后端下它不
是 FreeRTOS 任务调度。切到 FreeRTOS 后，入口结构要变成：

```text
main()
  -> SYSCFG_DL_init()
  -> app_main()              创建 LibXR 时间基准、PlatformInit 和任务
  -> vTaskStartScheduler()   FreeRTOS 接管调度

FreeRTOS tasks
  -> LibXR::Thread::Sleep()
  -> LibXR::Semaphore / Mutex / LockQueue / Timer / ASync
```

## 第 1 步：让 LibXR 选择 FreeRTOS 后端

把 `cmake/LibXR.CMake` 中的系统后端改成 FreeRTOS：

```cmake
set(LIBXR_SYSTEM FreeRTOS)
set(LIBXR_DRIVER mspm0)
set(LIBXR_NO_EIGEN True)
```

`libxr/cmake/config.cmake` 会把 `FreeRTOS` 归一化成小写 `freertos`，然后包含
`libxr/system/freertos/CMakeLists.txt`。这个后端会编译：

- `libxr/system/freertos/libxr_system.cpp`
- `libxr/system/freertos/thread.cpp`
- `libxr/system/freertos/semaphore.cpp`
- `libxr/system/freertos/mutex.cpp`
- `libxr/system/freertos/async.cpp`

同时会公开 `libxr/system/freertos` 这个 include 目录。

## 第 2 步：把 FreeRTOS Kernel 编进工程

LibXR 的 FreeRTOS 后端只封装 API，不会自动把 FreeRTOS Kernel 加到你的应用里。
你需要把 SDK 中的 FreeRTOS 源码纳入 CMake。

推荐在 `cmake/LibXR.CMake` 中加入类似下面的配置。路径基于当前仓库。
这段应放在 `MSPM0_ARCH_COMPILE_OPTIONS`、`MSPM0_COMMON_INCLUDE_DIRS` 和
`MSPM0_COMMON_COMPILE_DEFINITIONS` 都已经定义之后，并放在
`add_subdirectory("${LIBXR_DIR}" ...)` 之前：

```cmake
set(FREERTOS_DIR "${MSPM0_SDK_DIR}/kernel/freertos")
set(FREERTOS_SOURCE_DIR "${FREERTOS_DIR}/Source")
set(FREERTOS_PORT_DIR "${FREERTOS_SOURCE_DIR}/portable/GCC/ARM_CM0")
set(FREERTOS_MEMMANG_DIR "${FREERTOS_SOURCE_DIR}/portable/MemMang")
set(FREERTOS_CONFIG_DIR
    "${FREERTOS_DIR}/builds/LP_MSPM0G3507/release")

set(FREERTOS_INCLUDE_DIRS
    "${FREERTOS_CONFIG_DIR}"
    "${FREERTOS_SOURCE_DIR}/include"
    "${FREERTOS_PORT_DIR}")

set(FREERTOS_SOURCES
    "${FREERTOS_SOURCE_DIR}/list.c"
    "${FREERTOS_SOURCE_DIR}/queue.c"
    "${FREERTOS_SOURCE_DIR}/tasks.c"
    "${FREERTOS_SOURCE_DIR}/timers.c"
    "${FREERTOS_SOURCE_DIR}/event_groups.c"
    "${FREERTOS_SOURCE_DIR}/stream_buffer.c"
    "${FREERTOS_PORT_DIR}/port.c"
    "${FREERTOS_PORT_DIR}/portasm.c"
    "${FREERTOS_MEMMANG_DIR}/heap_4.c")

add_library(freertos_kernel STATIC ${FREERTOS_SOURCES})

target_include_directories(freertos_kernel PUBLIC
    ${FREERTOS_INCLUDE_DIRS}
    ${MSPM0_COMMON_INCLUDE_DIRS})

target_compile_options(freertos_kernel PUBLIC
    ${MSPM0_ARCH_COMPILE_OPTIONS})

target_compile_definitions(freertos_kernel PUBLIC
    ${MSPM0_COMMON_COMPILE_DEFINITIONS})
```

然后确保 `xr` 和应用都能看到 FreeRTOS 头文件：

```cmake
if(TARGET xr)
    target_include_directories(xr PUBLIC
        ${MSPM0_COMMON_INCLUDE_DIRS}
        ${FREERTOS_INCLUDE_DIRS})

    target_compile_options(xr PUBLIC
        ${MSPM0_ARCH_COMPILE_OPTIONS})

    target_compile_definitions(xr PUBLIC
        ${MSPM0_COMMON_COMPILE_DEFINITIONS})

    target_link_libraries(xr PUBLIC "${MSPM0_DRIVERLIB_A}")
endif()
```

根 `CMakeLists.txt` 中，应用目标需要链接 FreeRTOS Kernel：

```cmake
target_link_libraries(${PROJECT_NAME}.elf PRIVATE
    xr
    freertos_kernel)
```

同时从应用目标的编译定义中移除：

```cmake
LIBXR_NOT_SUPPORT_MUTI_THREAD=1
```

这个宏存在时，`LibXR::Timer` 不会创建后台定时器线程。

## 第 3 步：检查 FreeRTOSConfig.h

当前 SDK 已经提供 MSPM0G3507 的配置：

```text
mspm0-sdk/kernel/freertos/builds/LP_MSPM0G3507/release/FreeRTOSConfig.h
```

LibXR FreeRTOS 后端依赖下面这些条件：

| 配置项 | 要求 | 原因 |
| --- | --- | --- |
| `configTICK_RATE_HZ` | 必须是 `1000` | `libxr/system/freertos/libxr_system.cpp` 有静态断言；LibXR 的阻塞延时按毫秒理解 |
| `configMAX_PRIORITIES` | 至少 `6` | `Thread::Priority` 需要映射出 5 档优先级 |
| `configTICK_TYPE_WIDTH_IN_BITS` | 建议 32 位 | `TickType_t` 必须能容纳 LibXR 毫秒时间戳 |
| `configUSE_MUTEXES` | `1` | `LibXR::Mutex` 基于 FreeRTOS mutex |
| `configUSE_COUNTING_SEMAPHORES` | `1` | `LibXR::Semaphore` 基于 counting semaphore |
| `INCLUDE_vTaskDelay` | `1` | `LibXR::Thread::Sleep()` 调用 `vTaskDelay()` |
| `INCLUDE_vTaskDelayUntil` | `1` | `LibXR::Thread::SleepUntil()` 调用 `vTaskDelayUntil()` |
| `INCLUDE_xTaskGetCurrentTaskHandle` | `1` | `LibXR::Thread::Current()` 需要当前任务句柄 |

SDK 默认配置中：

```c
#define configTICK_RATE_HZ                1000
#define configMAX_PRIORITIES              10UL
#define configTOTAL_HEAP_SIZE             ((size_t)(3 * 1024))
#define configUSE_TIMERS                  1
#define configUSE_MUTEXES                 1
#define configUSE_COUNTING_SEMAPHORES     1
#define configUSE_TICK_HOOK               0
#define configUSE_TICKLESS_IDLE           1
```

建议先做两处调整：

```c
#define configTOTAL_HEAP_SIZE             ((size_t)(8 * 1024))
#define configUSE_TICKLESS_IDLE           0
```

`3 * 1024` 的 FreeRTOS heap 对多个任务、LibXR 队列、异步任务和 C++ 动态分配来说
通常偏小。先提高到 8 KB 更利于排除内存不足问题，稳定后再按 map 文件和
`xPortGetFreeHeapSize()` 回收。

`configUSE_TIMERS` 是 FreeRTOS 原生软件定时器开关。LibXR 的 `LibXR::Timer` 不依赖
它；LibXR 后端还会在该值为 1 时给出编译警告。如果你不用 FreeRTOS 原生
`xTimerCreate()`，可以设为 0，并从 `FREERTOS_SOURCES` 中移除 `timers.c`。

## 第 4 步：处理 SysTick 和 Timebase

这是当前 MSPM0 迁移时最容易踩的点。

当前 `libxr/driver/mspm0/mspm0_timebase.cpp` 定义了：

```cpp
extern "C" void SysTick_Handler(void)
{
  LibXR::MSPM0Timebase::OnSysTickInterrupt();
}
```

FreeRTOS 的 `portable/GCC/ARM_CM0/port.c` 也定义 `SysTick_Handler()`，并用它递增
RTOS tick、触发 PendSV 调度。如果两个文件都编译，会出现重复定义，或者更糟的是
只有一个 SysTick 逻辑生效。

推荐迁移方式：

1. FreeRTOS 接管 `SysTick_Handler()`。
2. 不再在 `main.c` 中手动调用 `DL_SYSTICK_init()`、`DL_SYSTICK_enableInterrupt()`、
   `DL_SYSTICK_enable()`。
3. 在 `mspm0_timebase.cpp` 中让裸机 SysTick handler 只在非 FreeRTOS 后端编译：

```cpp
#ifndef LIBXR_SYSTEM_freertos
extern "C" void SysTick_Handler(void)
{
  LibXR::MSPM0Timebase::OnSysTickInterrupt();
}
#endif
```

4. 如果你还需要 `LibXR::Timebase::GetMilliseconds()` 跟随 FreeRTOS tick，可以启用
   tick hook：

```c
#define configUSE_TICK_HOOK      1
#define configUSE_TICKLESS_IDLE  0
```

然后在一个会参与链接的 C++ 源文件中加入：

```cpp
#include "mspm0_timebase.hpp"

extern "C" void vApplicationTickHook(void)
{
  LibXR::MSPM0Timebase::OnSysTickInterrupt();
}
```

初次移植时建议关闭 tickless idle。因为 tickless idle 会在空闲时抑制一批 SysTick，
简单的 `OnSysTickInterrupt()` 每次加 1 无法自然覆盖被抑制的 tick。等系统稳定后，
再考虑用 FreeRTOS tick count 同步 `MSPM0Timebase::Sync()`，或者实现一个直接读取
`xTaskGetTickCount()` 的 RTOS 时间基准。

## 第 5 步：改 main.c 启动调度器

FreeRTOS 模式下，`main.c` 不再跑永久业务循环，而是初始化硬件、创建任务、启动
调度器。

示例结构：

```c
#include "app_main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"

int main(void)
{
  SYSCFG_DL_init();

  app_main();

  vTaskStartScheduler();

  while (1)
  {
  }
}
```

`vTaskStartScheduler()` 正常情况下不会返回。如果它返回，通常是 FreeRTOS heap 不足，
Idle 任务或 Timer 任务创建失败。

## 第 6 步：在 app_main.cpp 中创建 LibXR 任务

FreeRTOS 模式下，`app_main()` 应该完成这些事情：

1. 创建全局或静态的 `MSPM0Timebase`。
2. 调用 `LibXR::PlatformInit()`。
3. 初始化 GPIO、UART 等驱动对象。
4. 创建一个或多个 `LibXR::Thread`。
5. 返回给 `main()`，由 `vTaskStartScheduler()` 启动调度。

注意：当前裸机示例中的 `LibXR::MSPM0Timebase timebase;` 是局部对象，因为
`app_main()` 永不返回，所以还能工作。FreeRTOS 下 `app_main()` 会返回，因此
`timebase` 必须是静态或全局对象。

最小 LED 任务示例：

```cpp
#include "app_main.h"

#include "libxr.hpp"
#include "mspm0_gpio.hpp"
#include "mspm0_timebase.hpp"
#include "ti_msp_dl_config.h"

namespace
{
LibXR::MSPM0GPIO* led_gpio = nullptr;
LibXR::Thread led_thread;

void LedTask(void*)
{
  bool led_on = false;

  while (true)
  {
    led_on = !led_on;
    (void)led_gpio->Write(led_on);
    LibXR::Thread::Sleep(500);
  }
}
}  // namespace

extern "C" void app_main()
{
  static LibXR::MSPM0Timebase timebase;
  LibXR::PlatformInit();

  static LibXR::MSPM0GPIO led(GPIO_GRP_0_PORT, GPIO_GRP_0_PIN_0_PIN,
                              GPIO_GRP_0_PIN_0_IOMUX);
  led_gpio = &led;

  (void)led.SetConfig(
      {LibXR::GPIO::Direction::OUTPUT_PUSH_PULL, LibXR::GPIO::Pull::NONE});

  led_thread.Create<void*>(nullptr, LedTask, "led", 512,
                           LibXR::Thread::Priority::LOW);
}
```

这里的 `512` 是 LibXR 的线程栈深度，单位是字节。LibXR FreeRTOS 后端会把它换算成
FreeRTOS 的 stack word 数：

```cpp
uint32_t stack_size = stack_depth / 4;
if (stack_depth % 4 != 0)
{
  stack_size += 1;
}
```

所以不要再像直接调用 `xTaskCreate()` 那样把它理解成 word。

## 第 7 步：线程 API

`LibXR::Thread` 是 FreeRTOS task 的薄封装。

创建线程：

```cpp
LibXR::Thread worker;

void WorkerTask(void*)
{
  while (true)
  {
    // do work
    LibXR::Thread::Sleep(10);
  }
}

worker.Create<void*>(nullptr, WorkerTask, "worker", 768,
                     LibXR::Thread::Priority::MEDIUM);
```

优先级映射来自 `libxr/system/freertos/thread.hpp`：

```cpp
#define LIBXR_PRIORITY_STEP ((configMAX_PRIORITIES - 1) / 5)
```

在 SDK 默认 `configMAX_PRIORITIES = 10` 时，LibXR 优先级大致映射为：

| LibXR 优先级 | FreeRTOS 数值 |
| --- | --- |
| `IDLE` | 0 |
| `LOW` | 1 |
| `MEDIUM` | 2 |
| `HIGH` | 3 |
| `REALTIME` | 4 |

FreeRTOS 数值越大，优先级越高。LibXR 只使用 5 档，留出空间给你直接创建的
FreeRTOS 原生任务。

延时：

```cpp
LibXR::Thread::Sleep(100);
```

周期任务建议使用 `SleepUntil()`：

```cpp
void PeriodicTask(void*)
{
  LibXR::MillisecondTimestamp last = LibXR::Thread::GetTime();

  while (true)
  {
    // 每 20 ms 执行一次
    LibXR::Thread::SleepUntil(last, 20);
  }
}
```

因为 LibXR 要求 `configTICK_RATE_HZ == 1000`，这里的参数可以按毫秒理解。

## 第 8 步：信号量 Semaphore

`LibXR::Semaphore` 基于 FreeRTOS counting semaphore。

普通任务之间同步：

```cpp
LibXR::Semaphore data_ready;

void ProducerTask(void*)
{
  while (true)
  {
    // 准备数据
    data_ready.Post();
    LibXR::Thread::Sleep(100);
  }
}

void ConsumerTask(void*)
{
  while (true)
  {
    if (data_ready.Wait() == LibXR::ErrorCode::OK)
    {
      // 处理数据
    }
  }
}
```

带超时等待：

```cpp
if (data_ready.Wait(50) == LibXR::ErrorCode::TIMEOUT)
{
  // 50 ms 内没有等到信号
}
```

中断或驱动回调中释放信号量：

```cpp
extern LibXR::Semaphore data_ready;

void SomeIrqHandler()
{
  data_ready.PostFromCallback(true);
}
```

如果不是中断上下文，只是普通回调，可以传 `false`：

```cpp
data_ready.PostFromCallback(false);
```

## 第 9 步：互斥锁 Mutex

`LibXR::Mutex` 基于 FreeRTOS mutex，适合保护共享资源，例如 UART 打印、共享状态。

推荐用 `LockGuard`，离开作用域自动解锁：

```cpp
LibXR::Mutex log_mutex;

void PrintLog()
{
  LibXR::Mutex::LockGuard lock(log_mutex);

  // 这里访问共享 UART、日志缓冲区或全局数据
}
```

手动加锁：

```cpp
if (log_mutex.Lock() == LibXR::ErrorCode::OK)
{
  // critical section
  log_mutex.Unlock();
}
```

非阻塞尝试：

```cpp
if (log_mutex.TryLock() == LibXR::ErrorCode::OK)
{
  // got lock
  log_mutex.Unlock();
}
```

不要在 ISR 中使用 mutex。中断里需要通知任务时，用 `Semaphore::PostFromCallback()` 或
`LockQueue::PushFromCallback()`。

## 第 10 步：队列 LockQueue

`LibXR::LockQueue<T>` 基于 FreeRTOS queue，适合任务间传数据。

示例：一个任务周期产生字节，另一个任务消费字节：

```cpp
LibXR::LockQueue<uint8_t> rx_queue(32);

void ProducerTask(void*)
{
  uint8_t value = 0;

  while (true)
  {
    (void)rx_queue.Push(value++);
    LibXR::Thread::Sleep(100);
  }
}

void ConsumerTask(void*)
{
  uint8_t value = 0;

  while (true)
  {
    if (rx_queue.Pop(value) == LibXR::ErrorCode::OK)
    {
      // 使用 value
    }
  }
}
```

`Push()` 是非阻塞的，队列满时返回 `ErrorCode::FULL`。

`Pop()` 默认无限等待：

```cpp
rx_queue.Pop(value);
```

也可以指定超时：

```cpp
if (rx_queue.Pop(value, 10) == LibXR::ErrorCode::EMPTY)
{
  // 10 ms 内没有数据
}
```

ISR 中推送：

```cpp
void SomeIrqHandler()
{
  const uint8_t byte = 0x55;
  (void)rx_queue.PushFromCallback(byte, true);
}
```

普通任务环境优先直接用 `Push()`，中断环境用 `PushFromCallback(..., true)`。

## 第 11 步：LibXR Timer

`LibXR::Timer` 是 LibXR 自己的周期任务调度器，不是 FreeRTOS 原生 software timer。

FreeRTOS 模式下，只要没有定义 `LIBXR_NOT_SUPPORT_MUTI_THREAD`，第一次
`Timer::Add()` 或 `Timer::Refresh()` 会创建后台线程：

```cpp
thread_handle_.Create<void*>(nullptr, RefreshThreadFunction, "libxr_timer_task",
                             stack_depth_, priority_);
```

`PlatformInit()` 可以设置这个后台线程的优先级和栈大小：

```cpp
LibXR::PlatformInit(
    static_cast<uint32_t>(LibXR::Thread::Priority::MEDIUM),
    512);
```

创建一个 1000 ms 周期任务：

```cpp
void HeartbeatTimer(void*)
{
  // 每秒执行一次
}

auto heartbeat = LibXR::Timer::CreateTask<void*>(HeartbeatTimer, nullptr, 1000);
LibXR::Timer::Add(heartbeat);
LibXR::Timer::Start(heartbeat);
```

修改周期：

```cpp
LibXR::Timer::SetCycle(heartbeat, 500);
```

停止：

```cpp
LibXR::Timer::Stop(heartbeat);
```

不要把 `LibXR::Timer` 和 FreeRTOS `xTimerCreate()` 混在一起理解。前者由 LibXR 的
`libxr_timer_task` 执行回调，后者由 FreeRTOS Timer Service Task 执行回调。

## 第 12 步：ASync 异步任务

`LibXR::ASync` 会创建一个名为 `async_job` 的后台线程。它适合把中断或回调里不适合
直接做的事情丢给任务上下文处理。

创建异步执行器：

```cpp
LibXR::ASync async_worker(768, LibXR::Thread::Priority::LOW);
```

提交任务：

```cpp
struct JobContext
{
  uint32_t counter;
};

JobContext job_context{0};

void RunAsyncJob(bool in_isr, JobContext* context, LibXR::ASync* async)
{
  (void)in_isr;
  (void)async;

  context->counter++;
  // 在 async_job 线程里执行耗时操作
}

auto job = LibXR::ASync::Job::Create(RunAsyncJob, &job_context);

if (async_worker.AssignJob(job) == LibXR::ErrorCode::BUSY)
{
  // 上一个任务还没完成
}
```

查询状态：

```cpp
switch (async_worker.GetStatus())
{
  case LibXR::ASync::Status::READY:
    break;
  case LibXR::ASync::Status::BUSY:
    break;
  case LibXR::ASync::Status::DONE:
    break;
}
```

从中断或驱动回调提交：

```cpp
async_worker.AssignJobFromCallback(job, true);
```

## 第 13 步：完整 MSPM0 示例结构

下面示例展示一个常见结构：

- LED 任务每 500 ms 翻转一次。
- UART 任务每 1000 ms 输出 heartbeat。
- UART echo 的具体读写 API 仍沿用当前 `MSPM0UART` 驱动模式。
- 用 mutex 保护 UART 写操作，避免多个任务同时写。

```cpp
#include "app_main.h"

#include "libxr.hpp"
#include "mspm0_gpio.hpp"
#include "mspm0_timebase.hpp"
#include "mspm0_uart.hpp"
#include "ti_msp_dl_config.h"

namespace
{
LibXR::MSPM0GPIO* led_gpio = nullptr;
LibXR::MSPM0UART* uart0 = nullptr;

LibXR::Mutex uart_mutex;
LibXR::Thread led_thread;
LibXR::Thread heartbeat_thread;

void LedTask(void*)
{
  bool led_on = false;

  while (true)
  {
    led_on = !led_on;
    (void)led_gpio->Write(led_on);
    LibXR::Thread::Sleep(500);
  }
}

void HeartbeatTask(void*)
{
  static const char heartbeat[] = "[freertos] heartbeat\r\n";
  LibXR::WriteOperation write_op;

  while (true)
  {
    {
      LibXR::Mutex::LockGuard lock(uart_mutex);
      (void)uart0->Write(
          {reinterpret_cast<const uint8_t*>(heartbeat), sizeof(heartbeat) - 1},
          write_op);
    }

    LibXR::Thread::Sleep(1000);
  }
}
}  // namespace

extern "C" void app_main()
{
  static LibXR::MSPM0Timebase timebase;
  LibXR::PlatformInit();

  static LibXR::MSPM0GPIO led(GPIO_GRP_0_PORT, GPIO_GRP_0_PIN_0_PIN,
                              GPIO_GRP_0_PIN_0_IOMUX);
  led_gpio = &led;
  (void)led.SetConfig(
      {LibXR::GPIO::Direction::OUTPUT_PUSH_PULL, LibXR::GPIO::Pull::NONE});

  static uint8_t uart_rx_stage_buffer[256];
  static LibXR::MSPM0UART uart(
      MSPM0_UART_INIT(UART_0, uart_rx_stage_buffer, sizeof(uart_rx_stage_buffer),
                      16, 512));
  uart0 = &uart;

  led_thread.Create<void*>(nullptr, LedTask, "led", 512,
                           LibXR::Thread::Priority::LOW);
  heartbeat_thread.Create<void*>(nullptr, HeartbeatTask, "uart_hb", 768,
                                 LibXR::Thread::Priority::LOW);
}
```

如果要把 UART 接收也拆成独立任务，可以沿用当前 `src/app_main.cpp` 里的
`ReadOperation` 轮询状态机，只是把 `while (true)` 放进 `UartRxTask()`，并用
`Thread::Sleep(1)` 让出 CPU。

## 常见问题

### 编译时报重复定义 SysTick_Handler

原因：`mspm0_timebase.cpp` 和 FreeRTOS `port.c` 都定义了 `SysTick_Handler()`。

处理：让 FreeRTOS 接管 SysTick，把 `mspm0_timebase.cpp` 里的裸机 handler 用
`#ifndef LIBXR_SYSTEM_freertos` 包起来。需要同步 LibXR 时间时，用
`vApplicationTickHook()`。

### vTaskStartScheduler() 返回了

通常是 FreeRTOS heap 不足。先增大：

```c
#define configTOTAL_HEAP_SIZE ((size_t)(8 * 1024))
```

如果 `configUSE_TIMERS=1`，FreeRTOS 还要创建 Timer Service Task，也会消耗 heap。
不用 FreeRTOS 原生 software timer 时可以关闭它。

### Thread::Sleep() 延时不对

检查：

```c
#define configTICK_RATE_HZ 1000
```

LibXR FreeRTOS 后端要求 tick 是 1 ms。

### 创建线程后断言失败

检查：

- `configMAX_PRIORITIES >= 6`
- FreeRTOS heap 是否足够
- 传给 `Thread::Create()` 的栈大小是否过小
- 任务函数是否异常返回

### Timer 没有运行

检查应用目标是否还定义了：

```cmake
LIBXR_NOT_SUPPORT_MUTI_THREAD=1
```

FreeRTOS 模式下应移除它。否则 `LibXR::Timer` 不会创建后台刷新线程。

### 任务里能不能使用 new/delete

FreeRTOS 后端在非 ESP 平台重载了全局 C++ `new/delete`，内部使用
`pvPortMalloc()` 和 `vPortFree()`。这意味着 C++ 动态分配消耗的是 FreeRTOS heap。
如果任务、队列或 LibXR 对象创建失败，优先检查 `configTOTAL_HEAP_SIZE`。

## 迁移检查清单

- `LIBXR_SYSTEM` 已设置为 `FreeRTOS`。
- 应用目标已经移除 `LIBXR_NOT_SUPPORT_MUTI_THREAD=1`。
- FreeRTOS Kernel 源码已经加入 CMake。
- `xr` 和应用目标都能 include 到 `FreeRTOS.h`。
- `FreeRTOSConfig.h` 来自 `LP_MSPM0G3507/release` 或已按 MSPM0G3507 调整。
- `configTICK_RATE_HZ == 1000`。
- `configMAX_PRIORITIES >= 6`。
- `configUSE_MUTEXES == 1`。
- `configUSE_COUNTING_SEMAPHORES == 1`。
- `main.c` 不再手动初始化 SysTick。
- FreeRTOS 的 `SysTick_Handler()` 没有和 `MSPM0Timebase` 的裸机 handler 冲突。
- `app_main()` 创建任务后返回。
- `MSPM0Timebase` 是静态或全局对象。
- `vTaskStartScheduler()` 被调用，并且正常不返回。

完成这些后，你就可以在 MSPM0G3507 上用 LibXR 的统一系统层 API 编写 FreeRTOS
多任务应用。
