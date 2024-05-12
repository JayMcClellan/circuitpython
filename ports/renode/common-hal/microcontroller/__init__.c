/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Scott Shawcroft for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/mphal.h"
#include "py/obj.h"

#include "common-hal/microcontroller/__init__.h"
#include "shared-bindings/microcontroller/__init__.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/microcontroller/Processor.h"
#include "supervisor/filesystem.h"
#include "supervisor/port.h"
#include "supervisor/shared/safe_mode.h"

void common_hal_mcu_delay_us(uint32_t delay) {
    mp_hal_delay_us(delay);
}

volatile uint32_t nesting_count = 0;
void common_hal_mcu_disable_interrupts(void) {
    // We don't use save_and_disable_interrupts() from the sdk because we don't want to worry about PRIMASK.
    // This is what we do on the SAMD21 via CMSIS.
    asm volatile ("cpsid i" : : : "memory");
    // __dmb();
    nesting_count++;
}

void common_hal_mcu_enable_interrupts(void) {
    if (nesting_count == 0) {
        // reset_into_safe_mode(SAFE_MODE_INTERRUPT_ERROR);
    }
    nesting_count--;
    if (nesting_count > 0) {
        return;
    }
    // __dmb();
    asm volatile ("cpsie i" : : : "memory");
}

static bool next_reset_to_bootloader = false;

void common_hal_mcu_on_next_reset(mcu_runmode_t runmode) {
    switch (runmode) {
        case RUNMODE_UF2:
        case RUNMODE_BOOTLOADER:
            next_reset_to_bootloader = true;
            break;
        case RUNMODE_SAFE_MODE:
            safe_mode_on_next_reset(SAFE_MODE_PROGRAMMATIC);
            break;
        default:
            break;
    }
}

void common_hal_mcu_reset(void) {
    filesystem_flush();
    if (next_reset_to_bootloader) {
        reset_to_bootloader();
    } else {
        reset_cpu();
    }
}

// The singleton microcontroller.Processor object, bound to microcontroller.cpu
// It currently only has properties, and no state.
#if CIRCUITPY_PROCESSOR_COUNT > 1
static const mcu_processor_obj_t processor0 = {
    .base = {
        .type = &mcu_processor_type,
    },
};

static const mcu_processor_obj_t processor1 = {
    .base = {
        .type = &mcu_processor_type,
    },
};

const mp_rom_obj_tuple_t common_hal_multi_processor_obj = {
    {&mp_type_tuple},
    CIRCUITPY_PROCESSOR_COUNT,
    {
        MP_ROM_PTR(&processor0),
        MP_ROM_PTR(&processor1)
    }
};
#endif

const mcu_processor_obj_t common_hal_mcu_processor_obj = {
    .base = {
        .type = &mcu_processor_type,
    },
};

// This maps MCU pin names to pin objects.
const mp_rom_map_elem_t mcu_pin_global_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_GPIO0), MP_ROM_PTR(&pin_GPIO0) },
    { MP_ROM_QSTR(MP_QSTR_GPIO1), MP_ROM_PTR(&pin_GPIO1) },
};
MP_DEFINE_CONST_DICT(mcu_pin_globals, mcu_pin_global_dict_table);

const mcu_pin_obj_t *mcu_get_pin_by_number(int number) {
    for (size_t i = 0; i < MP_ARRAY_SIZE(mcu_pin_global_dict_table); i++) {
        mcu_pin_obj_t *obj = MP_OBJ_TO_PTR(mcu_pin_global_dict_table[i].value);
        if (obj->base.type == &mcu_pin_type && obj->number == number) {
            return obj;
        }
    }
    return NULL;
}
