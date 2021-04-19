/**
 * @file ir-sensor.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Functions for the MLX90632 IR temperature sensor
 * @version 0.1
 * @date 2021-04-17
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "main.h"

/** RAK12003 MLX90632 I2C address */
#define MLX90632_ADDRESS 0x3A
/** MLX90632 library */
MLX90632 RAK_TempSensor;

/** For calculating the average standard of the measurments */
AvgStd tempSamples;

/**
 * @brief Initialize the IR temperature sensor
 * 
 * @return true if sensor was found
 * @return false if the sensor didn't response
 */
bool init_ir(void)
{
	MLX90632::status returnError;

	tempSamples = AvgStd();
	tempSamples.reset();

	// Initialize I2C
	Wire.begin();

	// MLX90632 init
	if (RAK_TempSensor.begin(MLX90632_ADDRESS, Wire, returnError) == true)
	{
		MYLOG("IR", "MLX90632 Init Succeed");
		return true;
	}
	else
	{
		MYLOG("IR", "MLX90632 Init Failed");
		return false;
	}
	RAK_TempSensor.continuousMode();
}

/**
 * @brief Measures temperature for 10 seconds
 * 
 * @return float average temperature in Celsius from 10 seconds measuring
 */
float measure_loop(void)
{
	// Wake up the sensor
	RAK_TempSensor.continuousMode();

	time_t max_measure_time = 10000;

	bool stop_measure = false;

	time_t measure_start = millis();

	tempSamples.reset();

	digitalWrite(LED_BUILTIN, HIGH);
	digitalWrite(LED_CONN, LOW);
	while (!stop_measure)
	{
		digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
		digitalWrite(LED_CONN, !digitalRead(LED_CONN));
		tempSamples.checkAndAddReading(RAK_TempSensor.getObjectTemp());

		// Stop after max_measure_time
		if ((millis()-measure_start) > max_measure_time)
		{
			stop_measure = true;
		}
		delay(10);
		display_busy((millis() - measure_start)/100);
	}
	digitalWrite(LED_BUILTIN, LOW);
	digitalWrite(LED_CONN, LOW);
	MYLOG("IR", "Result is %.2f", tempSamples.getMean());
	// Set the sensor back into sleep mode
	RAK_TempSensor.sleepMode();
	return tempSamples.getMean();
}

/**
 * @brief Do a single temperature measurement
 * 
 * @return float measured temperature in Celsius
 */
float measure_single(void)
{
	// Wake up the sensor
	RAK_TempSensor.continuousMode();
	float measure_result =  RAK_TempSensor.getObjectTemp();
	// Set the sensor back into sleep mode
	RAK_TempSensor.sleepMode();
	return measure_result;
}