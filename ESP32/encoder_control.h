// =========================================================================================== INFO

// ESP32 encoder control (Header File, C version)
// Author: dimakomplekt
// Description: Encoder control with async_await for debounce using pure C for embedded

// =========================================================================================== INFO


// =========================================================================================== DEFINES


#ifndef ENCODER_CONTROL_H
#define ENCODER_CONTROL_H

// Empty pin name define 
#define GPIO_PIN_NONE ((gpio_num_t)(-1))

// =========================================================================================== DEFINES


// =========================================================================================== IMPORT

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "driver/gpio.h"              


#include <my_libs/async_await/async_await.h> // Async await lib connection
#include <my_libs/button_control/button_contol.h> // Button control lib connection

// =========================================================================================== IMPORT


// =========================================================================================== TYPE DEFINITION SECTION

// Type: parameter_type
// Purpose: Main type names for the logic changes inside the parameters control by encoder rotation function
typedef enum {

    TYPE_UNS_INT,
    TYPE_INT,
    TYPE_UINT_8,
    TYPE_UINT_16,
    TYPE_UINT_32,
    TYPE_UINT_64,
    TYPE_FLOAT,

} parameter_type;


// Union: parameter_value_union
// Union for the parameter and the regulation variables values type translation 
typedef union {

    unsigned int    uns_int;
    int             int_val;
    uint8_t         u8;
    uint16_t        u16;
    uint32_t        u32;
    uint64_t        u64;
    float           f;

} parameter_value_union;


// Type: rotation side
// Purpose: controls the encoder rotation side influence on the controlled parameter value
typedef enum {

    CLOCKWISE,        // For the += step to the parameter value with clockwise rotation
    COUNTERCLOCKWISE, // For the -= step to the parameter value with clockwise rotation

} rotation_side;


// Type: rotation_overflow_mode
// Purpose: controls the logic of the parameter overflow with the rotation
typedef enum {

    LIMITATION,       // Maximal/minimal parameter value after maximal/minimal parameter value
    ROTATION,         // Maximal/minimal parameter value after minimal/maximal parameter value
    
} rotation_overflow_mode;

// =========================================================================================== TYPE DEFINITION SECTION


// =========================================================================================== STRUCT DEFINITION SECTION

// Struct: encoder_ctx
// Purpose: Stores the pin numbers and debounce delay context for the encoder control
typedef struct encoder_ctx
{

    gpio_num_t ENC_VCC; // VCC pin
    gpio_num_t ENC_GND; // GND pin
    gpio_num_t ENC_SW; // SW pin
    gpio_num_t ENC_DT; // DT pin
    gpio_num_t ENC_CLK; // CLK pin

    async_await_ctx ENC_AWAIT; // Async await context
    
    button_ctx sw_button; // SW button ctx by button_control library

    rotation_overflow_mode overflow_mode; // Overflow mode

    bool last_clk_state; // Last CLK pin state for the parameter control
    // Flag for the correct call of the par_type_converting function inside the enc_rotation_value_control function
    bool new_parameter_type;
    
    parameter_type controlled_parameter_type; // Holder for the current parameter type
    parameter_value_union parameter; // Curr type from value union
    parameter_value_union step; // Regulation step
    parameter_value_union min_val; // Minimum parameter value
    parameter_value_union max_val; // Maximum parameter value

} encoder_ctx;

// =========================================================================================== STRUCT DEFINITION SECTION


// =========================================================================================== API DECLARATION

// Function: encoder_ctx_default
// Purpose: Default encoder_ctx struct initializer macros for initialization function
static inline encoder_ctx encoder_ctx_default(void) {
    return (encoder_ctx){

        .ENC_VCC = GPIO_PIN_NONE,
        .ENC_GND = GPIO_PIN_NONE,
        .ENC_SW = GPIO_PIN_NONE,
        .ENC_DT = GPIO_PIN_NONE,
        .ENC_CLK = GPIO_PIN_NONE,
        .ENC_AWAIT = {0},
        .overflow_mode = LIMITATION,
        .last_clk_state = false,
        .new_parameter_type = true,
        .controlled_parameter_type = TYPE_FLOAT,
        .parameter = {0},
        .step = {0},
        .min_val = {0},
        .max_val = {0},

    };
}


// Function: encoder_initialization
// Purpose: Initialize the encoder pins and debounce delay context
// by the selected encoder, pins and async await context
void encoder_initialization(

    encoder_ctx *encoder,
    gpio_num_t vcc_pin,
    gpio_num_t gnd_pin,
    gpio_num_t sw_pin,
    gpio_num_t dt_pin,
    gpio_num_t clk_pin

);



// Function: enc_rotation_value_control
// Purpose: Control the parameters by the encoder rotation 
// by the selected encoder, rotation side, parameter, step, limits and debounce async await.
// Values should be the variables, transmitted by the links - as like (&parameter or &regulation_step)
void enc_rotation_value_control(
    
    encoder_ctx *encoder,
    rotation_side side,
    rotation_overflow_mode rotation_regime,
    void *parameter,
    parameter_type type,
    void *step,
    void *min_val,
    void *max_val

);


// Helper-function: par_type_converting
// Purpose: Translate the parameters for the current value control function by the selected data and parameter 
void par_type_converting(encoder_ctx *encoder, parameter_type type, void *parameter, void *step, void *min_val, void *max_val);


// Helper-function: regulation_values_changed
// Purpose: Checks if the values which were passed as arguments inside the enc_rotation_value_control have been changed
bool regulation_values_changed(encoder_ctx *encoder, parameter_type type, void *parameter, void *step, void *min_val, void *max_val);

// =========================================================================================== API DECLARATION

#endif // ENCODER_CONTROL_H
