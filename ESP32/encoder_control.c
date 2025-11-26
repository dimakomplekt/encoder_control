// =========================================================================================== INFO

// ESP32 encoder control (main File, C version)
// Author: dimakomplekt
// Description: Encoder control, linked with async_await library for debounce, using pure C for embedded use
// The using example could be find in the end of this file

// =========================================================================================== INFO


// =========================================================================================== IMPORT

#include <stdbool.h>
#include "soc/gpio_reg.h"
#include "soc/gpio_struct.h"


// Header import
#include "encoder_control.h"


// =========================================================================================== IMPORT


// =========================================================================================== DEFINES

// For FreeRTOS
#ifdef USE_FREERTOS
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
// For ordinary ESP32 workflow
#else
    #include <time.h>
    #include <stdint.h>
#endif

// =========================================================================================== DEFINES


// =========================================================================================== HELPER FUNCTIONS

// Low-level read function
static inline int fast_gpio_read(int pin)
{
    if (pin < 32)
        return (GPIO.in >> pin) & 0x1;
    else
        return (GPIO.in1.val >> (pin - 32)) & 0x1;
}

// =========================================================================================== HELPER FUNCTIONS


// =========================================================================================== API DEFINITION SECTION

// Creates and initializes an encoder context
void encoder_initialization(encoder_ctx *encoder,
    gpio_num_t vcc_pin,
    gpio_num_t gnd_pin,
    gpio_num_t sw_pin,
    gpio_num_t dt_pin,
    gpio_num_t clk_pin) {

    // Error handler
    if (!encoder) return;

    // Default context values with initialization by the function
    *encoder = encoder_ctx_default();
    
    // Pins adding into context
    encoder->ENC_VCC = vcc_pin;
    encoder->ENC_GND = gnd_pin;
    encoder->ENC_SW = sw_pin;
    encoder->ENC_DT = dt_pin;
    encoder->ENC_CLK = clk_pin;

    // Debounce await for the rotation
    encoder->ENC_AWAIT = async_await_ctx_default();

    // Pins setup, depending on the values passed as function arguments
    // Encoder VCC initialization
    if (encoder->ENC_VCC != GPIO_PIN_NONE)
    {
        gpio_set_direction(encoder->ENC_VCC, GPIO_MODE_OUTPUT);
        gpio_set_level(encoder->ENC_VCC, 1);
    }

    // Encoder GND initialization
    if (encoder->ENC_GND != GPIO_PIN_NONE)
    {
        gpio_set_direction(encoder->ENC_GND, GPIO_MODE_OUTPUT);
        gpio_set_level(encoder->ENC_GND, 0);
    }

    // Encoder SW initialization
    if (encoder->ENC_SW != GPIO_PIN_NONE)
    {
        encoder->sw_button = button_initialization(encoder->ENC_SW, GPIO_PULLUP_ONLY, NO_FIX);
    }

    // Encoder DT initialization
    if (encoder->ENC_DT != GPIO_PIN_NONE)
    {
        gpio_set_direction(encoder->ENC_DT, GPIO_MODE_INPUT);
        gpio_set_pull_mode(encoder->ENC_DT, GPIO_PULLUP_ONLY);
    }

    // Encoder CLK initialization
    if (encoder->ENC_CLK != GPIO_PIN_NONE)
    {
        gpio_set_direction(encoder->ENC_CLK, GPIO_MODE_INPUT);
        gpio_set_pull_mode(encoder->ENC_CLK, GPIO_PULLUP_ONLY);


        // Initial CLK state for the enc_rotation_value_control function
        encoder->last_clk_state = fast_gpio_read(encoder->ENC_CLK);
    }

    // Error handler
    else encoder->last_clk_state = 0;
}


// Any type stepped parameter value control by the encoder rotation with the different side logic and different limitation logic
void enc_rotation_value_control(encoder_ctx *encoder,
    rotation_side side,
    rotation_overflow_mode rotation_regime,
    void *parameter,
    parameter_type type,
    void *step,
    void *min_val,
    void *max_val) {
    
    // Error handler
    if (encoder->ENC_CLK == GPIO_PIN_NONE || encoder->ENC_DT == GPIO_PIN_NONE) return;
    
    // Type error handler with the type setup
    if (encoder->controlled_parameter_type != type) encoder->new_parameter_type = true;
    
    // Reset the values, regulated by encoder with type change
    if (encoder->new_parameter_type)
    {
        // Set the new type inside the encoder context
        encoder->controlled_parameter_type = type;

        // Data converting by new the selected type with saving inside the storage 
        par_type_converting(encoder, type, parameter, step, min_val, max_val);

        // Operation control flag change
        encoder->new_parameter_type = false;
    }

    // Regulation parameters switch logic with type error handler
    if (encoder->controlled_parameter_type == type && regulation_values_changed(encoder, type, parameter, step, min_val, max_val))
    {
        // Reset the parameters values with the additional type error processing
        par_type_converting(encoder, type, parameter, step, min_val, max_val);
    }   


    // ENCODER CONTROL LOOP START

    // Get the encoder clk pin state
    int clk_state = fast_gpio_read(encoder->ENC_CLK);

    // Compare current clk state with the last clk state value, and if it's changed
    if (clk_state != encoder->last_clk_state && clk_state == 1)
    {
        // If the code pass through the debounce await
        if (async_await(&encoder->ENC_AWAIT, 3, TIME_UNIT_MS, false))
        {
            // Get the encoder dt pin state
            int dt_state = fast_gpio_read(encoder->ENC_DT);

            // If we choose parameter increase with counterclockwise rotation
            if (side == CLOCKWISE)
            {
                // If we fixed the difference of dt state in compare with clk state
                if (dt_state != clk_state)
                {
                    // Summarize the step value and current parameter value by the current type
                    switch (encoder->controlled_parameter_type)
                    {
                        case TYPE_UNS_INT: encoder->parameter.uns_int += encoder->step.uns_int; break;
                        case TYPE_INT: encoder->parameter.int_val += encoder->step.int_val; break;
                        case TYPE_UINT_8: encoder->parameter.u8 += encoder->step.u8; break;
                        case TYPE_UINT_16: encoder->parameter.u16 += encoder->step.u16; break;
                        case TYPE_UINT_32: encoder->parameter.u32 += encoder->step.u32; break;
                        case TYPE_UINT_64: encoder->parameter.u64 += encoder->step.u64; break;
                        case TYPE_FLOAT: encoder->parameter.f += encoder->step.f; break;
                    }
                }
                // If we fixed the equality of dt state in compare with clk state
                else
                {
                    // Subtract the step value from the current parameter value by the current type
                    // Switch with low-side limitation for LIMITATION overflow setting (else - we don't care 
                    // about subtraction overflow for the unsigned types, cause it rotates to the maximal limit anyway)
                    switch (encoder->controlled_parameter_type)
                    {
                        case TYPE_UNS_INT:
                            if (rotation_regime == LIMITATION)
                            {
                                if (encoder->parameter.uns_int <= encoder->step.uns_int)
                                    encoder->parameter.uns_int = encoder->min_val.uns_int;
                                else
                                    encoder->parameter.uns_int -= encoder->step.uns_int;
                            }
                            else
                            {
                                encoder->parameter.uns_int -= encoder->step.uns_int;
                            }
                            break;
                        
                        // Signed type don't requires a low-side limitation for LIMITATION overflow setting
                        case TYPE_INT:
                            encoder->parameter.int_val -= encoder->step.int_val;
                            break;
                    
                        case TYPE_UINT_8:
                            if (rotation_regime == LIMITATION)
                            {
                                if (encoder->parameter.u8 <= encoder->step.u8)
                                    encoder->parameter.u8 = encoder->min_val.u8;
                                else
                                    encoder->parameter.u8 -= encoder->step.u8;
                            }
                            else
                            {
                                encoder->parameter.u8 -= encoder->step.u8;
                            }
                            break;
                    
                        case TYPE_UINT_16:
                            if (rotation_regime == LIMITATION)
                            {
                                if (encoder->parameter.u16 <= encoder->step.u16)
                                    encoder->parameter.u16 = encoder->min_val.u16;
                                else
                                    encoder->parameter.u16 -= encoder->step.u16;
                            }
                            else
                            {
                                encoder->parameter.u16 -= encoder->step.u16;
                            }
                            break;
                    
                        case TYPE_UINT_32:
                            if (rotation_regime == LIMITATION)
                            {
                                if (encoder->parameter.u32 <= encoder->step.u32)
                                    encoder->parameter.u32 = encoder->min_val.u32;
                                else
                                    encoder->parameter.u32 -= encoder->step.u32;
                            }
                            else
                            {
                                encoder->parameter.u32 -= encoder->step.u32;
                            }
                            break;
                    
                        case TYPE_UINT_64:
                            if (rotation_regime == LIMITATION)
                            {
                                if (encoder->parameter.u64 <= encoder->step.u64)
                                    encoder->parameter.u64 = encoder->min_val.u64;
                                else
                                    encoder->parameter.u64 -= encoder->step.u64;
                            }
                            else
                            {
                                encoder->parameter.u64 -= encoder->step.u64;
                            }
                            break;
                        
                        // Signed type don't requires a low-side limitation for LIMITATION overflow setting
                        case TYPE_FLOAT:
                            encoder->parameter.f -= encoder->step.f;
                            break;
                    }
                    // printf("[ENCODER DEBUG] NEW LOWER PARAMETER VALUE. Param=%d", encoder->parameter.int_val);
                }
            }

            // Same logic for the parameter increase with the clockwise rotation
            else if (side == COUNTERCLOCKWISE)
            {
                if (dt_state != clk_state)
                {
                    // Switch with low-side limits
                    switch (encoder->controlled_parameter_type)
                    {
                        case TYPE_UNS_INT:
                            if (rotation_regime == LIMITATION)
                            {
                                if (encoder->parameter.uns_int <= encoder->step.uns_int)
                                    encoder->parameter.uns_int = encoder->min_val.uns_int;
                                else
                                    encoder->parameter.uns_int -= encoder->step.uns_int;
                            }
                            else
                            {
                                encoder->parameter.uns_int -= encoder->step.uns_int;
                            }
                            break;
                    
                        case TYPE_INT:
                            encoder->parameter.int_val -= encoder->step.int_val;
                            break;
                    
                        case TYPE_UINT_8:
                            if (rotation_regime == LIMITATION)
                            {
                                if (encoder->parameter.u8 <= encoder->step.u8)
                                    encoder->parameter.u8 = encoder->min_val.u8;
                                else
                                    encoder->parameter.u8 -= encoder->step.u8;
                            }
                            else
                            {
                                encoder->parameter.u8 -= encoder->step.u8;
                            }
                            break;
                    
                        case TYPE_UINT_16:
                            if (rotation_regime == LIMITATION)
                            {
                                if (encoder->parameter.u16 <= encoder->step.u16)
                                    encoder->parameter.u16 = encoder->min_val.u16;
                                else
                                    encoder->parameter.u16 -= encoder->step.u16;
                            }
                            else
                            {
                                encoder->parameter.u16 -= encoder->step.u16;
                            }
                            break;
                    
                        case TYPE_UINT_32:
                            if (rotation_regime == LIMITATION)
                            {
                                if (encoder->parameter.u32 <= encoder->step.u32)
                                    encoder->parameter.u32 = encoder->min_val.u32;
                                else
                                    encoder->parameter.u32 -= encoder->step.u32;
                            }
                            else
                            {
                                encoder->parameter.u32 -= encoder->step.u32;
                            }
                            break;
                    
                        case TYPE_UINT_64:
                            if (rotation_regime == LIMITATION)
                            {
                                if (encoder->parameter.u64 <= encoder->step.u64)
                                    encoder->parameter.u64 = encoder->min_val.u64;
                                else
                                    encoder->parameter.u64 -= encoder->step.u64;
                            }
                            else
                            {
                                encoder->parameter.u64 -= encoder->step.u64;
                            }
                            break;
                    
                        case TYPE_FLOAT:
                            encoder->parameter.f -= encoder->step.f;
                            break;
                    }
                }
                else
                {
                    switch (encoder->controlled_parameter_type)
                    {
                        case TYPE_UNS_INT: encoder->parameter.uns_int += encoder->step.uns_int; break;
                        case TYPE_INT: encoder->parameter.int_val += encoder->step.int_val; break;
                        case TYPE_UINT_8: encoder->parameter.u8 += encoder->step.u8; break;
                        case TYPE_UINT_16: encoder->parameter.u16 += encoder->step.u16; break;
                        case TYPE_UINT_32: encoder->parameter.u32 += encoder->step.u32; break;
                        case TYPE_UINT_64: encoder->parameter.u64 += encoder->step.u64; break;
                        case TYPE_FLOAT: encoder->parameter.f += encoder->step.f; break;
                    }
                }
            }
        }
    

        // Limitations setup 
        // If we choose to obtain the current limit value with limit overflow
        if (rotation_regime == LIMITATION)
        {
            switch (encoder->controlled_parameter_type)
            {
                // Limitation by the current parameter value compare with selected limits for selected data type
                case TYPE_UNS_INT:
                    if ((int)encoder->parameter.uns_int < (int)encoder->min_val.uns_int)
                        encoder->parameter.uns_int = encoder->min_val.uns_int;

                    if ((int)encoder->parameter.uns_int > (int)encoder->max_val.uns_int)
                        encoder->parameter.uns_int = encoder->max_val.uns_int;
                    break;
        
                case TYPE_INT:
                    if (encoder->parameter.int_val < encoder->min_val.int_val)
                        encoder->parameter.int_val = encoder->min_val.int_val;
                    if (encoder->parameter.int_val > encoder->max_val.int_val)
                        encoder->parameter.int_val = encoder->max_val.int_val;
                    break;
    
                case TYPE_UINT_8:
                    if ((int)encoder->parameter.u8 < (int)encoder->min_val.u8)
                        encoder->parameter.u8 = encoder->min_val.u8;

                    if ((int)encoder->parameter.u8 > (int)encoder->max_val.u8)
                        encoder->parameter.u8 = encoder->max_val.u8;
                    break;

                case TYPE_UINT_16:
                    if ((long)encoder->parameter.u16 < (long)encoder->min_val.u16)
                        encoder->parameter.u16 = encoder->min_val.u16;

                    if ((long)encoder->parameter.u16 > (long)encoder->max_val.u16)
                        encoder->parameter.u16 = encoder->max_val.u16;
                    break;
        
                case TYPE_UINT_32:
                    if ((long long)encoder->parameter.u32 < (long long)encoder->min_val.u32)
                        encoder->parameter.u32 = encoder->min_val.u32;

                    if ((long long)encoder->parameter.u32 > (long long)encoder->max_val.u32)
                        encoder->parameter.u32 = encoder->max_val.u32;
                    break;
        
                case TYPE_UINT_64:
                    if ((double)encoder->parameter.u64 < (double)encoder->min_val.u64)
                        encoder->parameter.u64 = encoder->min_val.u64;

                    if ((double)encoder->parameter.u64 > (double)encoder->max_val.u64)
                        encoder->parameter.u64 = encoder->max_val.u64;
                    break;
        
                case TYPE_FLOAT:
                    if (encoder->parameter.f < encoder->min_val.f)
                        encoder->parameter.f = encoder->min_val.f;
                    if (encoder->parameter.f > encoder->max_val.f)
                        encoder->parameter.f = encoder->max_val.f;
                    break;
            }
        }

        // If we choose to obtain the other limit value with limit overflow        
        else if (rotation_regime == ROTATION)
        {
            switch (encoder->controlled_parameter_type)
            {
                // Value change to other limit by the current parameter value compare with selected limits for selected data type
                case TYPE_UNS_INT:
                    if ((int)encoder->parameter.uns_int < (int)encoder->min_val.uns_int)
                        encoder->parameter.uns_int = encoder->max_val.uns_int;
                    if ((int)encoder->parameter.uns_int > (int)encoder->max_val.uns_int)
                        encoder->parameter.uns_int = encoder->min_val.uns_int;
                    break;
        
                case TYPE_INT:
                    if (encoder->parameter.int_val < encoder->min_val.int_val)
                        encoder->parameter.int_val = encoder->max_val.int_val;
                    if (encoder->parameter.int_val > encoder->max_val.int_val)
                        encoder->parameter.int_val = encoder->min_val.int_val;
                    break;
        
                case TYPE_UINT_8:
                    if ((int)encoder->parameter.u8 < (int)encoder->min_val.u8)
                        encoder->parameter.u8 = encoder->max_val.u8;
                    if ((int)encoder->parameter.u8 > (int)encoder->max_val.u8)
                        encoder->parameter.u8 = encoder->min_val.u8;
                    break;

                case TYPE_UINT_16:
                    if ((int)encoder->parameter.u16 < (int)encoder->min_val.u16)
                        encoder->parameter.u16 = encoder->max_val.u16;
                    if ((int)encoder->parameter.u16 > (int)encoder->max_val.u16)
                        encoder->parameter.u16 = encoder->min_val.u16;
                    break;
        
                case TYPE_UINT_32:
                    if ((long long)encoder->parameter.u32 < (long long)encoder->min_val.u32)
                        encoder->parameter.u32 = encoder->max_val.u32;
                    if ((long long)encoder->parameter.u32 > (long long)encoder->max_val.u32)
                        encoder->parameter.u32 = encoder->min_val.u32;
                    break;
        
                case TYPE_UINT_64:
                    if ((double)encoder->parameter.u64 < (double)encoder->min_val.u64)
                        encoder->parameter.u64 = encoder->max_val.u64;
                    if ((double)encoder->parameter.u64 > (double)encoder->max_val.u64)
                        encoder->parameter.u64 = encoder->min_val.u64;
                    break;
        
                case TYPE_FLOAT:
                    if (encoder->parameter.f < encoder->min_val.f)
                        encoder->parameter.f = encoder->max_val.f;
                    if (encoder->parameter.f > encoder->max_val.f)
                        encoder->parameter.f = encoder->min_val.f;
                    break;
            }
        }

        // Update the parameter value by the link with dependence from selected data type 
        switch (type) {
            case TYPE_UNS_INT: *(unsigned int*)parameter = encoder->parameter.uns_int; break;
            case TYPE_INT:     *(int*)parameter = encoder->parameter.int_val; break;
            case TYPE_UINT_8: *(uint8_t*)parameter = encoder->parameter.u8; break;
            case TYPE_UINT_16: *(uint16_t*)parameter = encoder->parameter.u16; break;
            case TYPE_UINT_32: *(uint32_t*)parameter = encoder->parameter.u32; break;
            case TYPE_UINT_64: *(uint64_t*)parameter = encoder->parameter.u64; break;
            case TYPE_FLOAT:   *(float*)parameter = encoder->parameter.f; break;
        }
    }
    // Switch the encoder clk state to the last state value
    encoder->last_clk_state = clk_state;

    // ENCODER CONTROL LOOP END
}


// Helper function, which swap the controlled input parameter type (void default), by the user selected parameter type from parameter_type structure
// Calculation called only for the first function call, or after parameter type swap 
void par_type_converting(encoder_ctx *encoder, parameter_type type, void *parameter, void *step, void *min_val, void *max_val)
{
    // Arguments error handler
    if (!parameter || !step || !min_val || !max_val) return;

    // Parameter type switch by the selected type
    switch (type) {
        case TYPE_UNS_INT:
            encoder->parameter.uns_int = *(unsigned int*)parameter;
            encoder->step.uns_int = *(unsigned int*)step;
            encoder->min_val.uns_int = *(unsigned int*)min_val;
            encoder->max_val.uns_int = *(unsigned int*)max_val;
            
            break;

        case TYPE_INT:
            encoder->parameter.int_val = *(int*)parameter;
            encoder->step.int_val = *(int*)step;
            encoder->min_val.int_val = *(int*)min_val;
            encoder->max_val.int_val = *(int*)max_val;

            break;

        case TYPE_UINT_8:
            encoder->parameter.u8 = *(uint8_t*)parameter;
            encoder->step.u8 = *(uint8_t*)step;
            encoder->min_val.u8 = *(uint8_t*)min_val;
            encoder->max_val.u8 = *(uint8_t*)max_val;

            break;

        case TYPE_UINT_16:
            encoder->parameter.u16 = *(uint16_t*)parameter;
            encoder->step.u16 = *(uint16_t*)step;
            encoder->min_val.u16 = *(uint16_t*)min_val;
            encoder->max_val.u16 = *(uint16_t*)max_val;

            break;

        case TYPE_UINT_32:
            encoder->parameter.u32 = *(uint32_t*)parameter;
            encoder->step.u32 = *(uint32_t*)step;
            encoder->min_val.u32 = *(uint32_t*)min_val;
            encoder->max_val.u32 = *(uint32_t*)max_val;

            break;

        case TYPE_UINT_64:
            encoder->parameter.u64 = *(uint64_t*)parameter;
            encoder->step.u64 = *(uint64_t*)step;
            encoder->min_val.u64 = *(uint64_t*)min_val;
            encoder->max_val.u64 = *(uint64_t*)max_val;

            break;

        case TYPE_FLOAT:
            encoder->parameter.f = *(float*)parameter;
            encoder->step.f = *(float*)step;
            encoder->min_val.f = *(float*)min_val;
            encoder->max_val.f = *(float*)max_val;

            break;

        default:
            break;
    }
}


// Helper function, which checks the changes of the enc_rotation_value_control function parameters 
bool regulation_values_changed(encoder_ctx *encoder, parameter_type type, void *parameter, void *step, void *min_val, void *max_val)
{
    // Arguments error handler
    if (!encoder || !parameter || !step || !min_val || !max_val) return false;

    // Changes check by selected type 
    switch(type)
    {
        case TYPE_UNS_INT:
            return (encoder->parameter.uns_int != *(unsigned int*)parameter) ||
                   (encoder->step.uns_int != *(unsigned int*)step) ||
                   (encoder->min_val.uns_int != *(unsigned int*)min_val) ||
                   (encoder->max_val.uns_int != *(unsigned int*)max_val);

        case TYPE_INT:
            return (encoder->parameter.int_val != *(int*)parameter) ||
                   (encoder->step.int_val != *(int*)step) ||
                   (encoder->min_val.int_val != *(int*)min_val) ||
                   (encoder->max_val.int_val != *(int*)max_val);

        case TYPE_UINT_8:
            return (encoder->parameter.u8 != *(uint8_t*)parameter) ||
                    (encoder->step.u8 != *(uint8_t*)step) ||
                    (encoder->min_val.u8 != *(uint8_t*)min_val) ||
                    (encoder->max_val.u8 != *(uint8_t*)max_val);

        case TYPE_UINT_16:
            return (encoder->parameter.u16 != *(uint16_t*)parameter) ||
                   (encoder->step.u16 != *(uint16_t*)step) ||
                   (encoder->min_val.u16 != *(uint16_t*)min_val) ||
                   (encoder->max_val.u16 != *(uint16_t*)max_val);

        case TYPE_UINT_32:
            return (encoder->parameter.u32 != *(uint32_t*)parameter) ||
                   (encoder->step.u32 != *(uint32_t*)step) ||
                   (encoder->min_val.u32 != *(uint32_t*)min_val) ||
                   (encoder->max_val.u32 != *(uint32_t*)max_val);

        case TYPE_UINT_64:
            return (encoder->parameter.u64 != *(uint64_t*)parameter) ||
                   (encoder->step.u64 != *(uint64_t*)step) ||
                   (encoder->min_val.u64 != *(uint64_t*)min_val) ||
                   (encoder->max_val.u64 != *(uint64_t*)max_val);

        case TYPE_FLOAT:
            return (encoder->parameter.f != *(float*)parameter) ||
                   (encoder->step.f != *(float*)step) ||
                   (encoder->min_val.f != *(float*)min_val) ||
                   (encoder->max_val.f != *(float*)max_val);

        default:
            return false;
    }
}

// =========================================================================================== API DEFINITION SECTION


// =========================================================================================== USING EXAMPLE SECTION

/*

#include <my_libs/async_await/async_await.h>
#include <my_libs/button_control/button_contol.h>
#include <my_libs/encoder_control/encoder_control.h>

encoder_ctx encoder;
button_ctx mode_button;

uint32_t value = 100;
uint32_t step = 1;
uint32_t min_val = 0;
uint32_t max_val = 1000;

bool mode_pressed = false;

void app_main() {
    // Initialize encoder (SW + DT + CLK)
    encoder_initialization(
        &encoder,
        GPIO_PIN_NONE,   // default VCC
        GPIO_PIN_NONE,   // default GND
        GPIO_NUM_13,     // SW
        GPIO_NUM_12,     // DT
        GPIO_NUM_14      // CLK
    );

    // Mode button init
    mode_button = button_initialization(GPIO_NUM_5, GPIO_PULLUP_ONLY, NO_FIX);

    while (1)
    {
        // One-time button press
        flag_control_by_but_onetime_press(&mode_button, &mode_pressed);

        if (mode_pressed) {
            // change step 1 / 10 / 100
            if (step == 1) step = 10;
            else if (step == 10) step = 100;
            else step = 1;
        }

        // Value control by encoder rotation
        enc_rotation_value_control(
            &encoder,
            CLOCKWISE,
            LIMITATION,
            &value,
            TYPE_UINT_32,
            &step,
            &min_val,
            &max_val
        );

        printf("Value: %lu   Step: %lu\n", value, step);

        // Async-friendly (no RTOS needed)
        await(500, TIME_UNIT_US);
    }
}
    
*/

// =========================================================================================== USING EXAMPLE SECTION
