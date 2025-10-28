# mqtt5 ESP32 MQTT Publisher

**mqtt5** is an ESP32 application that connects to a Wi-Fi network and publishes random humidity values to an MQTT broker (Adafruit IO). This project demonstrates how to use the ESP-IDF framework to create a simple MQTT client that publishes messages periodically.

## Features

- Connects to a specified Wi-Fi network.
- Establishes a connection with an MQTT broker using provided credentials.
- Publishes random humidity values (from `"000"` to `"100"`) every 5 seconds.
- Subscribes to a topic to receive messages (can be expanded for more functionality).

## Table of Contents

- [Requirements](#requirements)
- [Setup](#setup)
- [Configuration](#configuration)
- [Building and Flashing](#building-and-flashing)
- [Usage](#usage)
- [Code Overview](#code-overview)
- [License](#license)
- [Contributing](#contributing)
- [Acknowledgments](#acknowledgments)

## Requirements

- **Hardware:**
  - ESP32 development board.
- **Software:**
  - [ESP-IDF](https://github.com/espressif/esp-idf) development framework (version compatible with your ESP32).
  - Python 3.6 or higher (for ESP-IDF build system).
- **Accounts:**
  - An account on [Adafruit IO](https://io.adafruit.com/) (or another MQTT broker of your choice).
- **Network:**
  - Access to a 2.4 GHz Wi-Fi network.

## Setup

1. **Install ESP-IDF:**
   - Follow the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) to set up the development environment.

2. **Clone the Repository:**
   ```bash
   git clone https://github.com/yourusername/mqtt5.git
   cd mqtt5
   ```
3. **Set Up Adafruit IO (Optional):**

    If using Adafruit IO, create a new feed for humidity data.
    Obtain your Adafruit IO username and Active Key (API Key).

## Configuration

```bash
#define WIFI_SSID       \"Your_WiFi_SSID\"
#define WIFI_PASS       \"Your_WiFi_Password\"

#define MQTT_BROKER_URI \"mqtt://io.adafruit.com:1883\"
#define MQTT_USERNAME   \"Your_Adafruit_IO_Username\"
#define MQTT_PASSWORD   \"Your_Adafruit_IO_Key\"
#define MQTT_TOPIC      \"Your_Adafruit_IO_Feed\"
```