/**
 * @file main.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Setup and main loop
 * @version 0.1
 * @date 2021-04-17
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "main.h"

/** Semaphore used by events to wake up loop task */
SemaphoreHandle_t g_task_sem = NULL;

/** Priority required to give semaphore from an interrupt */
BaseType_t xHigherPriorityTaskWoken = pdTRUE;

/** Flag for the event type */
uint16_t g_task_event_type = NO_EVENT;

/** Timer to switch off the display */
SoftwareTimer oled_off;

/**
 * @brief IRQ callback when the button is pushed.
 *    We do not de-bouncing here, after first trigger we detach the IRQ
 */
void button_trigger(void)
{

	g_task_event_type = BUTTON;
	xSemaphoreGiveFromISR(g_task_sem, &xHigherPriorityTaskWoken);
	detachInterrupt(WB_IO1);
}

/**
 * @brief Arduino setup function
 * 
 */
void setup(void)
{
	// Initialize the built in LED
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	// Initialize the connection status LED
	pinMode(LED_CONN, OUTPUT);
	digitalWrite(LED_CONN, HIGH);

	// Initialize OLED
	init_display();
	oled_off.begin(DISPLAY_INIT_TIME, display_off, NULL, false);
	oled_off.start();
	display_status((char *)"POWER", true);
	display_status((char *)"ON", false);
	display_batt();

#if MY_DEBUG > 0
	// Initialize Serial for debug output
	Serial.begin(115200);

	time_t serial_timeout = millis();
	// On nRF52840 the USB serial is not available immediately
	while (!Serial)
	{
		if ((millis() - serial_timeout) < 5000)
		{
			delay(100);
			digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
		}
		else
		{
			break;
		}
	}
#endif

	digitalWrite(LED_BUILTIN, LOW);

	MYLOG("APP", "=====================================");
	MYLOG("APP", "RAK4631 IR thermometer");
	MYLOG("APP", "=====================================");

	// We are not using LoRa here
	// But to keep power consumption low we need
	// to initialize the radio
	lora_rak4630_init();
	// And send it to sleep mode
	Radio.Sleep();
	lora_hardware_uninit();

	// Initialize the IR thermometer chip
	if (!init_ir())
	{
		MYLOG("SETUP", "Could not find sensor");
		oled_off.stop();
		while (1)
		{
			// Print error message on the screen
			digitalWrite(LED_BUILTIN, HIGH);
			digitalWrite(LED_CONN, LOW);
			display_status((char *)"SENSOR", true);
			display_status((char *)"ERROR", false);
			delay(1000);
			// Clear screen
			digitalWrite(LED_BUILTIN, LOW);
			digitalWrite(LED_CONN, HIGH);
			display_status((char *)"CHECK", true);
			display_status((char *)"SENSOR", false);
			delay(1000);
		}
	}

	// Initialize BLE
	init_ble();

	// Create the task event semaphore
	g_task_sem = xSemaphoreCreateBinary();
	// Initialize semaphore
	xSemaphoreGive(g_task_sem);

	pinMode(WB_IO1, INPUT_PULLUP);
	attachInterrupt(WB_IO1, button_trigger, FALLING);

	digitalWrite(LED_CONN, LOW);

#if MY_DEBUG > 0
	// Delay just to show debug output
	delay(500);
#endif
	// Take the semaphore so the loop will go to sleep until an event happens
	xSemaphoreTake(g_task_sem, 10);
}

/**
 * @brief Arduino loop task
 * 
 */
void loop(void)
{
	// Sleep until we are woken up by an event
	if (xSemaphoreTake(g_task_sem, portMAX_DELAY) == pdTRUE)
	{
		// Switch on green LED to show we are awake
		digitalWrite(LED_BUILTIN, HIGH);
		while (g_task_event_type != NO_EVENT)
		{
			if ((g_task_event_type & BUTTON) == BUTTON)
			{
				g_task_event_type &= N_BUTTON;
				// Button pushed, start measurement
				MYLOG("APP", "Button push detected");
				digitalWrite(LED_CONN, HIGH);
				oled_off.stop();
				tone(WB_IO2, 698); //play the note "F6" (FA5)
				delay(100);
				tone(WB_IO2, 880); //play the note "A5" (LA5)
				delay(100);
				noTone(WB_IO2);

				display_on();
				display_status((char *)"START", true);
				display_status((char *)"MEASURE", false);
				display_batt();

				// Start measurement
				float measured_result = measure_loop();
				char result_str[32];
				sprintf(result_str, "%.2f ºC", measured_result);
				display_clear();
				display_status((char *)"Temp:", true);
				display_status(result_str, false);
				display_batt();

				for (int idx = 0; idx < 3; idx++)
				{
					tone(WB_IO2, 880); //play the note "A5" (LA5)
					delay(100);
					tone(WB_IO2, 698); //play the note "F6" (FA5)
				}
				delay(100);
				noTone(WB_IO2);

				digitalWrite(LED_CONN, LOW);
				attachInterrupt(WB_IO1, button_trigger, FALLING);
				oled_off.setPeriod(DISPLAY_OFF_TIME);
				oled_off.start();
			}
			if ((g_task_event_type & STATUS) == STATUS)
			{
				g_task_event_type &= N_STATUS;
				// OLED timeout, shut down
				MYLOG("APP", "Display timeout");
			}
			if ((g_task_event_type & BLE_DATA) == BLE_DATA)
			{
				g_task_event_type &= N_BLE_DATA;
				if (htm_active)
				{
					htm_indicate_temp();
				}
			}
			if ((g_task_event_type & BLE_START_DATA) == BLE_START_DATA)
			{
				g_task_event_type &= N_BLE_START_DATA;
				digitalWrite(LED_BUILTIN, HIGH);
				digitalWrite(LED_CONN, LOW);
				while (htm_active)
				{
					htm_indicate_temp();
					delay(1000);
					digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
					digitalWrite(LED_CONN, !digitalRead(LED_CONN));
				}
				digitalWrite(LED_BUILTIN, LOW);
				digitalWrite(LED_CONN, LOW);
			}
		}
		MYLOG("APP", "Loop goes to sleep");
		g_task_event_type = 0;
		// Go back to sleep
		xSemaphoreTake(g_task_sem, 10);
		// Switch off green LED to show we go to sleep
		digitalWrite(LED_BUILTIN, LOW);
		delay(10);
	}
}