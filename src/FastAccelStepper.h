#ifndef FASTACCELSTEPPER_H
#define FASTACCELSTEPPER_H
#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_ESP32) || defined(TEST)
#ifndef TEST
#include <Arduino.h>
#else
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "stubs.h"
#endif
#include <stdint.h>

#include "PoorManFloat.h"

#define TEST_MEASURE_ISR_SINGLE_FILL 0
#define TEST_CREATE_QUEUE_CHECKSUM 0

#if defined(TEST)
#define MAX_STEPPER 2
#define TICKS_PER_S 16000000L
#endif
#if defined(ARDUINO_ARCH_AVR)
#define MAX_STEPPER 2
#define TICKS_PER_S F_CPU
#endif
#if defined(ARDUINO_ARCH_ESP32)
#define MAX_STEPPER 6
#define TICKS_PER_S 16000000L
#endif

#define MIN_DELTA_TICKS (F_CPU / 50000)
#define ABSOLUTE_MAX_TICKS (255L * 65535)

class FastAccelStepper {
 public:
  // This should be only called by FastAccelStepperEngine !
  void init(uint8_t num, uint8_t step_pin);

  // step pin is defined at creation. Here can retrieve the pin
  uint8_t getStepPin();

  // if direction pin is connected, call this function
  void setDirectionPin(uint8_t dirPin);

  // if enable pin is connected, then use this function
  void setEnablePin(uint8_t enablePin);

  // using enableOutputs/disableOutputs the stepper can be enabled
  void enableOutputs();
  void disableOutputs();

  // in auto enable mode, the stepper is enabled before stepping and disabled
  // afterwards
  void setAutoEnable(bool auto_enable);

  // Retrieve the current position of the stepper - either in standstill or
  // while moving
  //    for esp32: the position while moving may deviate by the currently
  //    executed queue command's steps
  int32_t getCurrentPosition();

  // Set the current position of the stepper - either in standstill or while
  // moving.
  //    for esp32: the implementation uses getCurrentPosition(), which does not
  //               consider the steps of the current command
  //               => recommend to use only in standstill
  void setCurrentPosition(int32_t new_pos);

  // is true while the stepper is running
  bool isRunning();

  // For stepper movement control by FastAccelStepper
  //
  // setSpeed expects as parameter the minimum time between two steps.
  // If for example 5 steps/s shall be the maximum speed of the stepper,
  // then this call will be
  //      t = 0.2 s/steps = 200000 us/step
  //      setSpeed(200000);
  //
  // New value will be used after call to move/moveTo/stopMove
  //
  void setSpeed(uint32_t min_step_us);

  //  set Acceleration expects as parameter the change of speed
  //  as step/s².
  //  If for example the speed should ramp up from 0 to 10000 steps/s within
  //  10s, then the acceleration is 10000 steps/s / 10s = 1000 steps/s²
  //
  // New value will be used after call to move/moveTo/stopMove
  //
  void setAcceleration(uint32_t step_s_s);

  // start/move the stepper for (move) steps
  void move(int32_t move);

  // start/move the stepper to the absolute position
  void moveTo(int32_t position);

  // stop the running stepper as fast as possible with deceleration
  void stopMove();

  // get the target position for the current move
  int32_t targetPos();

  // Low level acccess via command queue
  // stepper queue management (low level access)
  int addQueueEntry(uint32_t start_delta_ticks, uint8_t steps, bool dir_high);

  // Return codes for addQueueEntry
#define AQE_OK 0
#define AQE_FULL -1
#define AQE_TOO_HIGH -2
#define AQE_TOO_LOW -3
#define AQE_STEPS_ERROR -4

  // check function s for command queue being empty or full
  bool isQueueEmpty();
  bool isQueueFull();

  // Get the future position of the stepper after all commands in queue are
  // completed
  int32_t getPositionAfterCommandsCompleted();

  // Set the future position of the stepper after all commands in queue are
  // completed. This has immediate effect to getCurrentPosition().
  void setPositionAfterCommandsCompleted(int32_t new_pos);

  // to be deprecated
  void addQueueStepperStop();
  bool isStopped();

  // This function provides info, in which state the high level stepper control
  // is operating
#define RAMP_STATE_IDLE 0
#define RAMP_STATE_ACCELERATE 1
#define RAMP_STATE_DECELERATE_TO_STOP 2
#define RAMP_STATE_DECELERATE 3
#define RAMP_STATE_COAST 4
  uint8_t rampState();

  // returns true, if the ramp generation is active
  bool isrSpeedControlEnabled();

  // This variable/these functions should NEVER be modified/called by the
  // application
  inline void isr_fill_queue();
  inline void isr_single_fill_queue();

#if (TEST_MEASURE_ISR_SINGLE_FILL == 1)
  uint32_t max_micros;
#endif
#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
  uint8_t checksum;
#endif

 private:
  uint8_t _rampState;  // updated by isr_fill_queue
  bool _isr_speed_control_enabled;
  int32_t _target_pos;
  uint8_t _stepPin;
  uint8_t _dirPin;
  uint8_t _auto_enablePin;
  uint8_t _enablePin;

  uint32_t _min_step_us;  // updated by setSpeed
  uint32_t _accel;        // updated by setAcceleration

  void _calculate_move(int32_t steps);
  void _update_from_speed_acceleration();

  // stepper_num's meaning depends on processor type:
  //		AVR:	0 => OC1B
  //		    	1 => OC1A
  //		ESP32:	0 => MCPWM0 Timer 0
  //				1 => MCPWM0 Timer 1
  //				2 => MCPWM0 Timer 2
  //				3 => MCPWM1 Timer 0
  //				4 => MCPWM1 Timer 1
  //				5 => MCPWM1 Timer 2
  uint8_t _stepper_num;
  uint8_t _queue_num;

  uint32_t _min_travel_ticks;  // in ticks, means 0.25us
  uint32_t _ramp_steps;        // ramp steps from 0 to max speed

  // used in interrupt routine isr_update_move
  uint32_t _deceleration_start;  // in steps
  upm_float _upm_inv_accel2;

  uint32_t _performed_ramp_up_steps;
};

class FastAccelStepperEngine {
 public:
  // stable API functions
  void init();

  // ESP32:
  // The first three steppers use mcpwm0, the next three steppers use mcpwm1
  //
  // AVR:
  // Only the pins connected to OC1A and OC1B are allowed
  //
  // If no stepper resources available or pin is wrong, then NULL is returned
  FastAccelStepper* stepperConnectToPin(uint8_t step_pin);

#if defined(ARDUINO_ARCH_AVR)
#define stepperA() stepperConnectToPin(9)
#define stepperB() stepperConnectToPin(10)
#endif

  // unstable API functions
  //
  // If this is called, then the periodic task will let the associated LED
  // blink with 1 Hz
  void setDebugLed(uint8_t ledPin);

  // This should be only called from ISR or stepper task
  void manageSteppers();

 private:
  uint8_t _next_stepper_num;
  FastAccelStepper* _stepper[MAX_STEPPER];

  bool _isValidStepPin(uint8_t step_pin);
};
#else
#error “This library only supports boards with an AVR or ESP32 processor.”
#endif
#endif
