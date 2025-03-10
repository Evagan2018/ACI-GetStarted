/*
 * Copyright (c) 2009-2020 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

//#include "app_cfg.h"
#include "RTE_Components.h"

#include "platform.h"

#if defined(__IS_COMPILER_IAR__) && __IS_COMPILER_IAR__
#   undef RTE_Compiler_IO_STDOUT_EVR
#endif

#ifdef RTE_CMSIS_View_EventRecorder
#   include "EventRecorder.h"
#   if defined(RTE_CMSIS_View_Fault_Record) || defined(RTE_CMSIS_View_Fault_Storage)
#       include "ARM_Fault.h"
#   endif
#endif

#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wunknown-warning-option"
#   pragma clang diagnostic ignored "-Wreserved-identifier"
#   pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#   pragma clang diagnostic ignored "-Wformat-nonliteral"
#   pragma clang diagnostic ignored "-Wsign-compare"
#   pragma clang diagnostic ignored "-Wmissing-prototypes"
#   pragma clang diagnostic ignored "-Wcast-qual"
#   pragma clang diagnostic ignored "-Wsign-conversion"
#   pragma clang diagnostic ignored "-Wgnu-statement-expression"
#   pragma clang diagnostic ignored "-Wundef"
#elif defined(__IS_COMPILER_GCC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wpedantic"
#endif


#if __IS_COMPILER_ARM_COMPILER_6__
static __inline__ int __attribute__((__always_inline__, __nodebug__))
__semihost(int val, const void *ptr) {
  register int v __asm__("r0") = val;
  register const void *p __asm__("r1") = ptr;
  __asm__ __volatile__(
#if defined(__thumb__)
#if defined(__ARM_ARCH_PROFILE) && __ARM_ARCH_PROFILE == 'M'
      "bkpt 0xab"
#else /* !defined(__ARM_ARCH_PROFILE) || __ARM_ARCH_PROFILE != 'M' */
#if defined(__USE_HLT_SEMIHOSTING)
      ".inst.n 0xbabc"
#else
      "svc 0xab"
#endif
#endif
#else /* !defined(__thumb__) */
#if defined(__USE_HLT_SEMIHOSTING)
      ".inst 0xe10f0070"
#else
      "svc 0x123456"
#endif
#endif
      : "+r"(v), "+r"(p)
      :
      : "memory", "cc");
  return v;
}
#endif

extern void SysTick_Handler(void);
extern void _ttywrch(int ch);

__WEAK 
void platform_1ms_event_handler(void) 
{}

__WEAK 
void bsp_1ms_event_handler(void)
{}


///// Setup timer hardware.
///// \return       status (1=Success, 0=Failure)
//uint32_t EventRecorderTimerSetup (void)
//{
//    return 1;
//}

///// Get timer frequency.
///// \return       timer frequency in Hz
//uint32_t EventRecorderTimerGetFreq (void)
//{
//    return SystemCoreClock;
//}

///// Get timer count.
///// \return       timer count (32-bit)
//uint32_t EventRecorderTimerGetCount (void)
//{
//    return get_system_ticks();
//}

#ifdef RTE_CMSIS_RTOS2_RTX5
void ARM_WRAP(osRtxTick_Handler)(void)
{
    platform_1ms_event_handler();
    bsp_1ms_event_handler();

#if __IS_COMPILER_GCC__ || __IS_COMPILER_LLVM__
    user_code_insert_to_systick_handler();
#endif

    extern void ARM_REAL(osRtxTick_Handler)(void);
    ARM_REAL(osRtxTick_Handler)();
}
#elif defined(RTE_CMSIS_RTOS2_FreeRTOS)

#else
__attribute__((used))
void SysTick_Handler(void)
{
    platform_1ms_event_handler();
    
    bsp_1ms_event_handler();
    
#if __IS_COMPILER_GCC__ || __IS_COMPILER_LLVM__ || __IS_COMPILER_IAR__
    user_code_insert_to_systick_handler();
#endif
}
#endif

/*----------------------------------------------------------------------------*
 * A thread safe printf                                                       *
 *----------------------------------------------------------------------------*/
#if defined(RTE_CMSIS_RTOS2)

#if defined(__IS_COMPILER_GCC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wpedantic"
#endif

#if defined(__IS_COMPILER_GCC)
_ATTRIBUTE ((__format__ (__printf__, 1, 2)))
#elif defined(__IS_ARM_COMPILER_5__) || defined(__IS_ARM_COMPILER_6__)
#pragma __printf_args
__attribute__((__nonnull__(1)))
#endif
int	ARM_WRAP(printf) (const char *__restrict format, ...)
{
    va_list va;
    va_start(va, format);
    int ret = 0;
    arm_thread_safe { 
        ret = vprintf(format, va);
    }
    va_end(va);
    
    return ret;
}

#endif


__WEAK 
bool device_specific_init(void)
{
    return false;
}

#if __IS_COMPILER_IAR__
__attribute__((used, constructor))
#else
__attribute__((used, constructor(101)))
#endif
void app_platform_init(void)
{

#ifdef RTE_CMSIS_View_EventRecorder
    EventRecorderInitialize(EventRecordAll, 1);
    EventRecorderClockUpdate();                   // Update Event Recorder timestamp clock 

#   if defined(RTE_CMSIS_View_Fault_Record) || defined(RTE_CMSIS_View_Fault_Storage)
    if (ARM_FaultOccurred() != 0U) {              // If fault information exists
      ARM_FaultRecord();                          // Output decoded fault information via Event Recorder
      EventRecorderStop();                        // Stop Event Recorder
    }
#   endif
#endif

    init_cycle_counter(device_specific_init());
}

#if __IS_COMPILER_ARM_COMPILER_6__
__asm(".global __use_no_semihosting\n\t");
#   ifndef __MICROLIB
__asm(".global __ARM_use_no_argv\n\t");
#   endif

__NO_RETURN
void _sys_exit(int ret)
{
    (void)ret;
    /* quit FVP */
#if defined(__USE_FVP__)
    __semihost(0x18, (const void *)0x20026);
#endif
    while(1) __NOP();
}

#endif

#if defined(__MICROLIB)
_ARMABI_NORETURN 
ARM_NONNULL(1,2)
void __aeabi_assert(const char *chCond, const char *chLine, int wErrCode) 
{
    ARM_2D_UNUSED(chCond);
    ARM_2D_UNUSED(chLine);
    ARM_2D_UNUSED(wErrCode);
    while(1) {
        __NOP();
    }
}
#endif

#if !defined(__IS_COMPILER_GCC__)
void _ttywrch(int ch)
{
    (void)ch;
}
#endif

#if defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(__IS_COMPILER_GCC__)
#   pragma GCC diagnostic pop
#endif
