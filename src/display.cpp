/**
 * @file display.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Display functions
 * @version 0.1
 * @date 2021-04-17
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "main.h"

/** Display class */
SSD1306Wire display(0x3c, WB_I2C1_SDA, WB_I2C1_SCL, GEOMETRY_128_64);

/** Battery level in mV */
float batt_level;
/** Millivolts per LSB 3.0V ADC range and 12-bit ADC resolution = 3000mV/4096 */
#define VBAT_MV_PER_LSB (0.73242188F)
/** Compensation factor for the VBAT divider */
#define VBAT_DIVIDER_COMP (1.73)
/** Real milli Volts per LSB including compensation */
#define REAL_VBAT_MV_PER_LSB (VBAT_DIVIDER_COMP * VBAT_MV_PER_LSB)

/**
 * @brief Initialize the display
 */
void init_display(void)
{
	delay(500); // Give display reset some time
	display.setI2cAutoInit(true);
	display.init();
	display.displayOff();
	display.clear();
	display.displayOn();
	// display.flipScreenVertically();
	display.setContrast(128);
	display.setFont(ArialMT_Plain_24);
	display.setTextAlignment(TEXT_ALIGN_CENTER);
	display.display();

	// Battery voltage reading initializing
	// Set the analog reference to 3.0V (default = 3.6V)
	analogReference(AR_INTERNAL_3_0);

	// Set the resolution to 12-bit (0..4095)
	analogReadResolution(12); // Can be 8, 10, 12 or 14
}

/**
 * @brief Clear the display content
 * 
 */
void display_clear(void)
{
	display.clear();
	display.display();
}

/**
 * @brief Display a text on the screen
 * 
 * @param disp_line Text as char[]
 * @param top_line if true write on upper display half, else on lower display half
 */
void display_status(char *disp_line, bool top_line)
{
	display.setFont(ArialMT_Plain_24);
	display.setTextAlignment(TEXT_ALIGN_CENTER);
	if (top_line)
	{
		display.drawString(64, 1, disp_line);
	}
	else
	{
		display.drawString(64, 28, disp_line);
	}
	display.display();
}

/**
 * @brief Write battery level status on the screen
 * 
 */
void display_batt(void)
{
	display.setFont(ArialMT_Plain_10);
	display.setTextAlignment(TEXT_ALIGN_RIGHT);
	char batt_level[16] = {0};
	sprintf(batt_level, "%.3fV", (readVBAT() / 1000.0));
	MYLOG("DIS", "Batt: %.3fV", (readVBAT() / 1000.0));
	display.drawString(127, 54, batt_level);
	display.display();
}

/**
 * @brief Show progress bar during measurment
 * 
 * @param progress 0-100% progress as uint8_t
 */
void display_busy(uint8_t progress)
{
	display.drawProgressBar(0, 54, 80, 9, progress);
	display.display();
}

/**
 * @brief Switch display on
 * 
 */
void display_on(void)
{
	display.clear();
	display.displayOn();
	display.display();
}

/**
 * @brief Switch display off to save battery
 * 
 * @param unused 
 */
void display_off(TimerHandle_t unused)
{
	display.clear();
	display.displayOff();
	display.display();
}

/**
 * @brief Read the analog value from the battery analog pin
 * and convert it to milli volt
 * 
 * @return float Battery level in milli volts
 */
float readVBAT(void)
{
	float raw;

	// Get the raw 12-bit, 0..3000mV ADC value
	raw = analogRead(WB_A0);

	// Convert the raw value to compensated mv, taking the resistor-
	// divider into account (providing the actual LIPO voltage)
	// ADC range is 0..3000mV and resolution is 12-bit (0..4095)
	return raw * REAL_VBAT_MV_PER_LSB;
}
