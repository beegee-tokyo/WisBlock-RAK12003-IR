/**
 * @file ble.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief BLE initialization & device configuration
 * @version 0.1
 * @date 2021-01-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "main.h"

void setupHTM(void);

/** OTA DFU service */
BLEDfu ble_dfu;
/** Device information service */
BLEDis ble_dis;
/* Health Thermometer Service Definitions
 * Health Thermometer Service:  0x1809
 * Temperature Measurement Char: 0x2A1C
 */
BLEService htms = BLEService(UUID16_SVC_HEALTH_THERMOMETER);
BLECharacteristic htmc = BLECharacteristic(UUID16_CHR_TEMPERATURE_MEASUREMENT);

/** Flag if HTM indication is active */
bool htm_active = false;

// Connect callback
void connect_callback(uint16_t conn_handle);
// Disconnect callback
void disconnect_callback(uint16_t conn_handle, uint8_t reason);
// Uart RX callback
void bleuart_rx_callback(uint16_t conn_handle);

/**
 * @brief Initialize BLE and start advertising
 * 
 */
void init_ble(void)
{
	// Config the peripheral connection with maximum bandwidth
	// more SRAM required by SoftDevice
	// Note: All config***() function must be called before begin()
	Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
	Bluefruit.configPrphConn(128, BLE_GAP_EVENT_LENGTH_MIN, 16, 16);

	// Start BLE
	Bluefruit.begin(1, 0);

	// Set max power. Accepted values are: (min) -40, -20, -16, -12, -8, -4, 0, 2, 3, 4, 5, 6, 7, 8 (max)
	Bluefruit.setTxPower(8);

	// Create device name
	char helper_string[256] = {0};

	uint32_t addr_high = ((*((uint32_t *)(0x100000a8))) & 0x0000ffff) | 0x0000c000;
	uint32_t addr_low = *((uint32_t *)(0x100000a4));
	/** Device name for RAK4631 */
	sprintf(helper_string, "RAK-HTM-%02X%02X%02X%02X%02X%02X",
			(uint8_t)(addr_high), (uint8_t)(addr_high >> 8), (uint8_t)(addr_low),
			(uint8_t)(addr_low >> 8), (uint8_t)(addr_low >> 16), (uint8_t)(addr_low >> 24));

	Bluefruit.setName(helper_string);

	// No BLE LED control
	Bluefruit.autoConnLed(false);

	// Set connection/disconnect callbacks
	Bluefruit.Periph.setConnectCallback(connect_callback);
	Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

	// Configure and Start Device Information Service
	ble_dis.setManufacturer("RAKwireless");

	ble_dis.setModel("RAK4631");

	sprintf(helper_string, "%d.%d.%d", SW_V_MAIN, SW_V_MED, SW_V_MIN);
	ble_dis.setSoftwareRev(helper_string);

	ble_dis.setHardwareRev("52840");

	ble_dis.begin();

	// Start the DFU service
	ble_dfu.begin();

	// Start the HTM service
	setup_htm();

	// Advertising packet
	Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE); //
	Bluefruit.Advertising.addService(htms);
	Bluefruit.Advertising.addName();
	Bluefruit.Advertising.addTxPower();

	/* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds 
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
	Bluefruit.Advertising.restartOnDisconnect(true);
	Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
	Bluefruit.Advertising.setFastTimeout(15);	// number of seconds in fast mode
	Bluefruit.Advertising.start(0);				// 0 = Don't stop advertising
}

/**
 * @brief  Callback when client connects
 * @param  conn_handle: Connection handle id
 */
void connect_callback(uint16_t conn_handle)
{
	(void)conn_handle;
	MYLOG("BLE", "Connected");
}

/**
 * @brief  Callback invoked when a connection is dropped
 * @param  conn_handle: connection handle id
 * @param  reason: disconnect reason
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
	(void)conn_handle;
	(void)reason;
	MYLOG("BLE", "Disconnected");
	htm_active = false;
}

/**
 * @brief Callback for CCCD
 * 
 * @param conn_hdl Connection handle
 * @param chr Pointer to characteristic
 * @param cccd_value CCCD value
 */
void cccd_callback(uint16_t conn_hdl, BLECharacteristic *chr, uint16_t cccd_value)
{
	// Display the raw request packet
	MYLOG("BLE", "CCCD Updated: %d", cccd_value);

	// Check the characteristic this CCCD update is associated with in case
	// this handler is used for multiple CCCD records.
	if (chr->uuid == htmc.uuid)
	{
		if (chr->indicateEnabled(conn_hdl))
		{
			MYLOG("BLE", "HTM indication enabled");
			// Wake up loop to start BLE HTM indication
			// Start task to read temperature every 1 second and indicate it
			htm_active = true;
			g_task_event_type = BLE_START_DATA;
			xSemaphoreGiveFromISR(g_task_sem, &xHigherPriorityTaskWoken);
		}
		else
		{
			MYLOG("BLE", "HTM indication disabled");
			// Stop BLE HTM indication
			htm_active = false;
		}
	}
}

/**
 * @brief Setup Health Thermometer Service
 * 
 */
void setup_htm(void)
{
	// Configure the Health Thermometer service
	// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.service.health_thermometer.xml
	// Supported Characteristics:
	// Name                         UUID    Requirement Properties
	// ---------------------------- ------  ----------- ----------
	// Temperature Measurement      0x2A1C  Mandatory   Indicate
	//
	// Temperature Type             0x2A1D  Optional    Read                  <-- Not used here
	// Intermediate Temperature     0x2A1E  Optional    Read, Notify          <-- Not used here
	// Measurement Interval         0x2A21  Optional    Read, Write, Indicate <-- Not used here
	htms.begin();

	// Note: You must call .begin() on the BLEService before calling .begin() on
	// any characteristic(s) within that service definition.. Calling .begin() on
	// a BLECharacteristic will cause it to be added to the last BLEService that
	// was 'begin()'ed!

	// Configure the Temperature Measurement characteristic
	// See:https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.temperature_measurement.xml
	// Properties = Indicte
	// Min Len    = 6
	// Max Len    = 6
	//    B0      = UINT8  - Flag (MANDATORY)
	//      b3:7  = Reserved
	//      b2    = Temperature Type Flag (0 = Not present, 1 = Present)
	//      b1    = Timestamp Flag (0 = Not present, 1 = Present)
	//      b0    = Unit Flag (0 = Celsius, 1 = Fahrenheit)
	//    B4:1    = FLOAT  - IEEE-11073 32-bit FLOAT measurement value
	//    B5      = Temperature Type
	htmc.setProperties(CHR_PROPS_INDICATE);
	htmc.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
	htmc.setFixedLen(6);
	htmc.setCccdWriteCallback(cccd_callback); // Optionally capture CCCD updates
	htmc.begin();
	uint8_t htmdata[6] = {0b00000100, 0, 0, 0, 0, 2}; // Set the characteristic to use Celsius, with type (body) but no timestamp field
	htmc.write(htmdata, sizeof(htmdata));			  // Use .write for init data

	// Temperature Type Value
	// See: https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.temperature_type.xml
	//    B0      = UINT8 - Temperature Type
	//      0     = Reserved
	//      1     = Armpit
	//      2     = Body (general)
	//      3     = Ear (usually ear lobe)
	//      4     = Finger
	//      5     = Gastro-intestinal Tract
	//      6     = Mouth
	//      7     = Rectum
	//      8     = Toe
	//      9     = Tympanum (ear drum)
	//     10:255 = Reserved
}

/**
 * @brief Get a single temperature measurement and send it over BLE by indicating
 * 
 */
void htm_indicate_temp(void)
{
	uint8_t htmdata[6] = {0b00000100, 0, 0, 0, 0, 2}; // Celsius unit, temperature type = body (2)

	double single_measure = measure_single();
	float2IEEE11073(single_measure, &htmdata[1]);
	// Note: We use .indicate instead of .write!
	// If it is connected but CCCD is not enabled
	// The characteristic's value is still updated although indicate is not sent
	if (htmc.indicate(htmdata, sizeof(htmdata)))
	{
		MYLOG("BLE", "Temperature Measurement updated to: %.2f", single_measure);
	}
	else
	{
		MYLOG("BLE", "ERROR: Indicate not set in the CCCD or not connected!");
	}
}