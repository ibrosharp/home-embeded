# Diagnostic Error Codes and LED Blink Patterns

When the LED Feedback setting is enabled, the device uses the onboard LED indicator to display critical system errors through a blink pattern system.

## Blink Engine Logic
If the system encounters an error, the LED indicator will enter diagnostic mode.
The blink pattern consists of:
1. **A long pause (2 seconds)**
2. **Slow blinks** (600ms ON, 400ms OFF) representing the **tens digit** of the error code.
3. **A medium pause (1 second)**
4. **Fast blinks** (200ms ON, 200ms OFF) representing the **ones digit** of the error code.
5. **A long pause (2 seconds)** before either repeating the code or moving to the next active error.

For example, an error code of **35** would be displayed as:
`3 SLOW blinks ... 5 FAST blinks`

If multiple errors occur simultaneously, the device will cycle through them.

## Supported Error Codes

| Error Description | Code | Blink Pattern | Mitigation / Action |
| --- | :---: | --- | --- |
| **Wi-Fi Connection Failure** | `12` | 1 Slow, 2 Fast | Check the router. The device has fallen back to the Config AP (`ESP32_Config_Node`). Connect to it to update Wi-Fi credentials. |
| **Storage Initialization Failure** | `13` | 1 Slow, 3 Fast | Fatal error. The internal flash memory (Preferences) could not be mounted. The device requires a hard reboot or flash reformat. |
| **I2C Expander Failure** | `21` | 2 Slow, 1 Fast | The MCP23X17 IO expander failed to initialize. Check the I2C wiring (SDA/SCL) and pull-up resistors. |
| **DHT Climate Sensor Failure** | `31` | 3 Slow, 1 Fast | The DHT22 sensor returned invalid data (NaN). Check the sensor wiring. The error will automatically clear when the sensor recovers. |
| **IR Subsystem Failure** | `41` | 4 Slow, 1 Fast | The IR Controller failed to start or encountered an error during transmission/reception. |
| **System Memory Low** | `51` | 5 Slow, 1 Fast | The device is running dangerously low on heap memory. A reboot may be required. |

> [!TIP]
> If there are no active errors, the LED is yielded back to the system to be used for short confirmation pulses during user actions (e.g., toggling a switch or saving a setting).
