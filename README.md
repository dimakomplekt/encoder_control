# encoder_control

A lightweight and extensible **encoder control library** for embedded systems   (ESP32, STM32, Arduino).   Provides easy handling of encoder rotation, direction detection, step filtering, and integrated button (SW) support.



This library focuses on clean encoder state tracking, portable context management, and minimal MCU overhead.



✨ Features: 

  * Minimalistic encoder rotation API  
  * Clockwise/counter-clockwise detection supporting.  
  * Built-in step filtering  
  * Optional SW-button integration via button_control library
  * Fully asynchronous (debounce and timing rely on async_await)  
  * Works without RTOS — no tasks, no threads, no delays  
  * Portable design intended for ESP32 and further - STM32 / Arduino  
  * Clean flag-based logic suitable for loops and state machines  



🚀 Basic Idea

This library allows you to read encoder rotation in a **non-blocking**, **fully asynchronous** way.

Core concepts:

- Each encoder has its own context (timestamps, states, filters)
- async_await is used internally for timing & filtering
- button_control can be attached to handle the SW click
- Flags are automatically updated for rotation events

This makes it easy to integrate encoders into UI logic, menu systems, and reactive embedded workflows.




⚠️ BIG WARNING ⚠️

  Required to download and link:

  ✔ async_await → required for asynchronous filtering and timing
  
  👉 https://github.com/dimakomplekt/async_await  
  
  ✔ button_control → required for handling the encoder’s SW button
  
  👉 https://github.com/dimakomplekt/button_control  

⚠️ BIG WARNING ⚠️




❓ Supported Data Types (`1.0`):

```c
typedef enum {

    TYPE_UNS_INT,
    TYPE_INT,
    TYPE_UINT_8,
    TYPE_UINT_16,
    TYPE_UINT_32,
    TYPE_UINT_64,
    TYPE_FLOAT,

} parameter_type;
```



More data types and value handling modes will be added in the future.



❓ Supported logic (`1.0`):

  ✔ Basic direction flags:
  
  - CW (clockwise)
  - CCW (counter-clockwise)
    
  ✔ Optional button support:
  
  - Short press  
  - Long press  
  - Single press flags  
  - SW callbacks planned  



⚡ Using Examples - AT THE END OF C-FILES


Initialization API looks like:

```c
// Button control library
#include <my_libs/button_control/button_contol.h>

// Encoder control library
#include <my_libs/encoder_control/encoder_control.h>


// Encoder 1 pins
#define ENC_DUTY_SW GPIO_NUM_13
#define ENC_DUTY_DT GPIO_NUM_12
#define ENC_DUTY_CLK GPIO_NUM_14
// Encoder 1 pins

// Encoder struct initialization
encoder_ctx encoder_1;

// PWM parameters
uint8_t duty_cycle = 10;
uint8_t duty_cycle_step = 1;
uint8_t duty_cycle_min = 2;
uint8_t duty_cycle_max = 15;

// SW control flags
bool encoder_1_pressed = false;


// Encoder initialization
encoder_initialization(&encoder_1,
    GPIO_PIN_NONE,                // Choose to use the default 5V pin for enc VCC
    GPIO_PIN_NONE,                // Choose to use the default GND pin for the enc GND
    ENC_DUTY_SW,                  // SW by the defined pin name
    ENC_DUTY_DT,                  // DT by the defined pin name
    ENC_DUTY_CLK                  // CLK by the defined pin name
);
```


⚡ Control API looks like:

```c
// Encoder SW-button press control
flag_control_by_but_onetime_press(&encoder_1.sw_button, &encoder_1_pressed);

if (encoder_1_pressed)
{
    // Your logic
}

// Control the duty cycle parameter by the encoder rotation
enc_rotation_value_control(&encoder_1,
    CLOCKWISE, 
    LIMITATION,
    &duty_cycle,
    TYPE_UINT_8,
    &duty_cycle_step,
    &duty_cycle_min,
    &duty_cycle_max
);
```



🫟 Current Version - Version: 1.0



⚠️ Known Limitations

  * Callback system is partially implemented, needs full integration (inside the button_control library)
  * Error handlers planned but not fully implemented


🛠 Future Plans

  * Full callback support
  * Extended data type support (float, fixed-point, bounded numeric ranges)
  * Hardware timer auto-selection (STM32 HAL/LL)


  🫵 You are welcome to help us make this library better!
