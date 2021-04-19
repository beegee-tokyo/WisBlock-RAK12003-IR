/**
 * @file main.h
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Main include file for libraries and definitions
 * @version 0.1
 * @date 2021-04-17
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <SX126x-Arduino.h>
#include <SPI.h>
#include <SparkFun_MLX90632_Arduino_Library.h>
#include <nRF_SSD1306Wire.h>
#include "avg.h"
#include <bluefruit.h>
#include "IEEE11073float.h"

// SW version
#define SW_V_MAIN 1 // Version number main
#define SW_V_MED 0	// Version number medium
#define SW_V_MIN 0	// Version number minor

// Debug output set to 0 to disable app debug output
#ifndef MY_DEBUG
#define MY_DEBUG 0
#endif

#if MY_DEBUG > 0
#define MYLOG(tag, ...)                  \
	do                                   \
	{                                    \
		if (tag)                         \
			Serial.printf("[%s] ", tag); \
		Serial.printf(__VA_ARGS__);      \
		Serial.printf("\n");             \
	} while (0)
#else
#define MYLOG(...)
#endif

/** Wake up events */
#define NO_EVENT 0
#define STATUS 0b0000000000000001
#define N_STATUS 0b1111111111111110
#define BLE_CONFIG 0b0000000000000010
#define N_BLE_CONFIG 0b1111111111111101
#define BLE_DATA 0b0000000000000100
#define N_BLE_DATA 0b1111111111111011
#define BLE_START_DATA 0b0000000000001000
#define N_BLE_START_DATA 0b1111111111110111
#define LIGHT 0b0000000000010000
#define N_LIGHT 0b1111111111101111
#define PIR_TRIGGER 0b0000000000100000
#define N_PIR_TRIGGER 0b1111111111011111
#define BUTTON 0b0000000001000000
#define N_BUTTON 0b1111111110111111

/** Semaphore used by events to wake up loop task */
extern SemaphoreHandle_t g_task_sem;

/** Flag for the event type */
extern uint16_t g_task_event_type;

/** Timer to switch off the OLED after some time */
extern SoftwareTimer oled_off;
/** Required for giving a semaphore from an IRQ handler */
extern BaseType_t xHigherPriorityTaskWoken;

// IR thermometer stuff
bool init_ir(void);
float measure_loop(void);
float measure_single(void);

/** Display stuff */
#define DISPLAY_INIT_TIME 5000
#define DISPLAY_OFF_TIME 30000
void init_display(void);
void display_clear(void);
void display_status(char *line, bool top_line);
void display_busy(uint8_t progress);
void display_on(void);
void display_off(TimerHandle_t unused);
void display_batt(void);
float readVBAT(void);

// BLE
#include <bluefruit.h>
void init_ble(void);
void setup_htm(void);
void htm_indicate_temp(void);
extern bool htm_active;
extern SoftwareTimer htm_timer;

#endif // MAIN_H