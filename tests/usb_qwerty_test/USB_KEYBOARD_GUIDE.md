# How to Read USB Keyboard Input on ESP32 (USB Host HID)

## Overview

This guide explains how to read USB keyboard input on ESP32-based devices using
the **USB Host** and **HID Host** drivers from ESP-IDF. This approach works on
any ESP32 chip with USB OTG support (ESP32-S2, ESP32-S3, ESP32-P4).

The key insight: your ESP32 acts as a **USB Host** (like a computer), and the
keyboard is a **USB Device**. The keyboard sends standard **HID reports** that
you parse to detect key presses.

## Hardware Requirements

- ESP32 with USB OTG support (ESP32-S2, ESP32-S3, or ESP32-P4)
- USB-A female port (or OTG adapter) connected to the ESP32's USB pins
- A standard USB keyboard
- **Important:** The USB-A port must provide 5V power to the keyboard

For M5Stack Tab5 specifically, the USB-A port is built-in with power control
via I/O expanders.

## Software Stack

```
Your Application
       |
   HID Host Driver      <-- espressif/usb_host_hid component
       |
   USB Host Library      <-- built into ESP-IDF
       |
   USB DWC HAL           <-- hardware abstraction
       |
   ESP32 USB OTG HW     <-- physical USB port
```

### Required Library

Only ONE external dependency:

```yaml
# main/idf_component.yml
dependencies:
  espressif/usb_host_hid: ^1.0.3
```

This automatically pulls in the `espressif/usb` component as well.

### Required Headers

```c
#include "usb/usb_host.h"    // USB Host Library (ESP-IDF built-in)
#include "usb/hid_host.h"    // HID Host driver (from espressif/usb_host_hid)
#include "usb/hid.h"         // HID definitions
```

## sdkconfig Settings

Add these to your `sdkconfig.defaults`:

```ini
# USB Host configuration
CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE=256
CONFIG_USB_HOST_HW_BUFFER_BIAS_BALANCED=y
CONFIG_USB_HOST_DEBOUNCE_DELAY_MS=250
```

## How USB HID Keyboard Works

A USB keyboard in **boot protocol** mode sends 8-byte reports:

```
Byte 0: Modifier keys (bitmask)
Byte 1: Reserved (always 0x00)
Byte 2: Key 1 (scancode)
Byte 3: Key 2 (scancode)
Byte 4: Key 3 (scancode)
Byte 5: Key 4 (scancode)
Byte 6: Key 5 (scancode)
Byte 7: Key 6 (scancode)
```

### Modifier Byte (Byte 0)

```
Bit 0: Left Ctrl
Bit 1: Left Shift
Bit 2: Left Alt
Bit 3: Left GUI (Win/Cmd)
Bit 4: Right Ctrl
Bit 5: Right Shift
Bit 6: Right Alt
Bit 7: Right GUI
```

### Scancodes (Bytes 2-7)

USB HID scancodes are NOT ASCII. They follow the USB HID Usage Tables:

| Scancode | Key    | Scancode | Key     |
|----------|--------|----------|---------|
| 0x04     | a      | 0x1E     | 1       |
| 0x05     | b      | 0x1F     | 2       |
| 0x06     | c      | 0x20     | 3       |
| ...      | ...    | ...      | ...     |
| 0x1D     | z      | 0x27     | 0       |
| 0x28     | Enter  | 0x29     | Escape  |
| 0x2A     | Backsp | 0x2B     | Tab     |
| 0x2C     | Space  | 0x39     | CapsLck |
| 0x3A-0x45| F1-F12 | 0x4F     | Right   |
| 0x50     | Left   | 0x51     | Down    |
| 0x52     | Up     |          |         |

When a key is **pressed**, its scancode appears in bytes 2-7.
When a key is **released**, its scancode disappears from the array.
Up to 6 keys can be pressed simultaneously (6-key rollover).

## Step-by-Step Implementation

### Step 1: Initialize USB Host

```c
// Install USB Host Library
usb_host_config_t host_config = {
    .skip_phy_setup = false,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
};
ESP_ERROR_CHECK(usb_host_install(&host_config));

// OR on M5Stack Tab5 (BSP handles USB Host setup):
ESP_ERROR_CHECK(bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true));
```

### Step 2: Register USB Host Client (for event monitoring)

```c
static usb_host_client_handle_t s_usb_client = NULL;

static void usb_event_cb(const usb_host_client_event_msg_t* msg, void* arg)
{
    switch (msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "New device connected (addr %d)", msg->new_dev.address);
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGI(TAG, "Device disconnected");
            break;
        default: break;
    }
}

usb_host_client_config_t client_config = {
    .is_synchronous = false,
    .max_num_event_msg = 5,
    .async = {
        .client_event_callback = usb_event_cb,
        .callback_arg = NULL
    }
};
usb_host_client_register(&client_config, &s_usb_client);

// Create a task to process client events
void usb_client_task(void* arg) {
    while (1) {
        usb_host_client_handle_events(s_usb_client, portMAX_DELAY);
    }
}
xTaskCreate(usb_client_task, "usb_cl", 4096, NULL, 5, NULL);
```

### Step 3: Install HID Host Driver

This is the core component that handles HID device enumeration:

```c
static void hid_device_cb(hid_host_device_handle_t dev,
                           const hid_host_driver_event_t event, void* arg);

const hid_host_driver_config_t hid_config = {
    .create_background_task = true,
    .task_priority = 5,
    .stack_size = 4096,
    .core_id = 0,
    .callback = hid_device_cb,      // Called when HID device connects
    .callback_arg = NULL
};
ESP_ERROR_CHECK(hid_host_install(&hid_config));
```

### Step 4: Handle HID Device Connection

When a HID device connects, the driver calls your callback:

```c
static void hid_iface_cb(hid_host_device_handle_t dev,
                          const hid_host_interface_event_t event, void* arg);

static void hid_device_cb(hid_host_device_handle_t dev,
                           const hid_host_driver_event_t event, void* arg)
{
    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        // Get device info
        hid_host_dev_info_t info;
        hid_host_get_device_info(dev, &info);
        ESP_LOGI(TAG, "HID Device VID:0x%04X PID:0x%04X", info.VID, info.PID);

        hid_host_dev_params_t params;
        hid_host_device_get_params(dev, &params);
        ESP_LOGI(TAG, "Protocol: %d (1=keyboard, 2=mouse)", params.proto);

        // Open device and start receiving reports
        const hid_host_device_config_t cfg = {
            .callback = hid_iface_cb,       // Called for each HID report
            .callback_arg = NULL
        };
        hid_host_device_open(dev, &cfg);
        hid_host_device_start(dev);
    }
}
```

### Step 5: Receive and Parse Keyboard Reports

This is where you actually read keyboard data:

```c
// Keyboard state
typedef struct {
    uint8_t modifier;
    uint8_t keys[6];
} kbd_state_t;

static kbd_state_t s_cur = {};
static kbd_state_t s_prev = {};

static void hid_iface_cb(hid_host_device_handle_t dev,
                          const hid_host_interface_event_t event, void* arg)
{
    if (event == HID_HOST_INTERFACE_EVENT_INPUT_REPORT) {
        uint8_t data[64] = {};
        size_t len = 0;

        esp_err_t ret = hid_host_device_get_raw_input_report_data(
            dev, data, sizeof(data), &len);

        if (ret == ESP_OK && len >= 8) {
            // Parse the 8-byte boot protocol report
            s_cur.modifier = data[0];
            // data[1] is reserved
            for (int i = 0; i < 6; i++) {
                s_cur.keys[i] = data[2 + i];
            }

            // Detect newly pressed keys
            for (int i = 0; i < 6; i++) {
                uint8_t sc = s_cur.keys[i];
                if (sc == 0) continue;

                bool was_pressed = false;
                for (int j = 0; j < 6; j++) {
                    if (s_prev.keys[j] == sc) {
                        was_pressed = true;
                        break;
                    }
                }

                if (!was_pressed) {
                    // NEW KEY PRESSED!
                    ESP_LOGI(TAG, "Key pressed: scancode 0x%02X", sc);
                }
            }

            // Detect released keys
            for (int i = 0; i < 6; i++) {
                uint8_t sc = s_prev.keys[i];
                if (sc == 0) continue;

                bool still_pressed = false;
                for (int j = 0; j < 6; j++) {
                    if (s_cur.keys[j] == sc) {
                        still_pressed = true;
                        break;
                    }
                }

                if (!still_pressed) {
                    ESP_LOGI(TAG, "Key released: scancode 0x%02X", sc);
                }
            }

            // Save current state as previous for next report
            memcpy(&s_prev, &s_cur, sizeof(kbd_state_t));
        }
    }

    if (event == HID_HOST_INTERFACE_EVENT_DISCONNECTED) {
        hid_host_device_close(dev);
    }
}
```

### Step 6: Convert Scancode to Character

```c
// US QWERTY layout: scancode 0x04..0x38 -> ASCII
static const char sc_to_ascii[] = {
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '1','2','3','4','5','6','7','8','9','0',
    '\n',0x1B,'\b','\t',' ',
    '-','=','[',']','\\','#',';','\'','`',',','.','/'
};

static const char sc_to_ascii_shift[] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '!','@','#','$','%','^','&','*','(',')',
    '\n',0x1B,'\b','\t',' ',
    '_','+','{','}','|','~',':','"','~','<','>','?'
};

char scancode_to_char(uint8_t scancode, uint8_t modifier) {
    if (scancode < 0x04 || scancode > 0x38) return 0;
    bool shifted = (modifier & 0x22) != 0;  // Left or Right Shift
    return shifted ? sc_to_ascii_shift[scancode - 0x04]
                   : sc_to_ascii[scancode - 0x04];
}
```

## Common Issues and Solutions

### 1. "Keyboard not detected"

- **Check power:** USB keyboards need 5V. Ensure your USB port supplies power.
- **Check USB Host init:** Make sure `usb_host_install()` or your BSP's
  USB Host start function is called before `hid_host_install()`.
- **Enable root port power:** Call `usb_host_lib_set_root_port_power(true)`.

### 2. "Getting reports but data looks wrong"

- Some keyboards prepend a **Report ID** byte before the standard 8 bytes.
  If byte[1] is always 0x00, it's likely standard boot protocol.
  If not, try `offset = 1` to skip the Report ID.

### 3. "Keyboard has multiple interfaces"

Most keyboards expose 2 HID interfaces:
- **Interface 0** (proto=1): Standard keyboard (boot protocol)
- **Interface 1** (proto=0): Media keys / Consumer control

You can check `params.proto == 1` to identify the keyboard interface.

### 4. Build error: `USB_DWC_HAL_PORT_EVENT_REMOTE_WAKEUP undeclared`

This happens with ESP-IDF v6.1. Fix by patching
`managed_components/espressif__usb/CMakeLists.txt`:

Replace the `REMOTE_WAKE_HAL_SUPPORTED` section at the end with:

```cmake
# PATCHED: Disable for IDF 6.1 compatibility
set(REMOTE_WAKE_HAL_SUPPORTED OFF)
```

### 5. "I want to use USB hub (keyboard through a hub)"

Add to `sdkconfig.defaults`:
```ini
CONFIG_USB_HOST_HUBS_SUPPORTED=y
CONFIG_USB_HOST_HUB_MULTI_LEVEL=y
```

## Project Structure

```
usb_qwerty_test/
├── CMakeLists.txt              # Project CMake (set project name)
├── sdkconfig.defaults          # ESP-IDF configuration
├── partitions.csv              # Partition table
├── components/
│   └── m5stack_tab5/           # BSP component (Tab5-specific)
├── main/
│   ├── CMakeLists.txt          # Component deps
│   ├── idf_component.yml       # espressif/usb_host_hid dependency
│   └── app_main.cpp            # Application code
└── managed_components/         # Auto-downloaded by IDF Component Manager
    ├── espressif__usb/
    └── espressif__usb_host_hid/
```

## Callback Flow Diagram

```
USB keyboard plugged in
        |
        v
usb_event_cb()  ---->  "New device connected"
        |
        v
hid_device_cb()  ---->  HID_HOST_DRIVER_EVENT_CONNECTED
        |                   |
        |                   v
        |            hid_host_device_open()
        |            hid_host_device_start()
        |
        v
hid_iface_cb()  ---->  HID_HOST_INTERFACE_EVENT_INPUT_REPORT
        |                   |
        |                   v
        |            hid_host_device_get_raw_input_report_data()
        |                   |
        |                   v
        |            Parse 8-byte report:
        |            [modifier][0x00][key1][key2][key3][key4][key5][key6]
        |
        v
   Your key handler (print, display, remap, etc.)
```

## Tested Hardware

| Device           | Chip      | Works | Notes                    |
|------------------|-----------|-------|--------------------------|
| M5Stack Tab5     | ESP32-P4  | Yes   | Built-in USB-A port      |
| M5Stack Cardputer| ESP32-S3  | Yes   | Needs OTG adapter        |
| Generic ESP32-S3 | ESP32-S3  | Yes   | Wire USB D+/D- pins      |

| Keyboard         | VID:PID       | Works | Notes              |
|------------------|---------------|-------|--------------------|
| HP USB Keyboard  | 03F0:6941     | Yes   | 2 HID interfaces   |
| PS5 DualSense    | 054C:0CE6     | Yes   | Gamepad, not kbd   |

## Full Source Code

The complete working project is available in this repository:
`cardputer/tab5/tests/usb_qwerty_test/`

## Credits

- **Andy** ([@AndyAiCardputer](https://github.com/AndyAiCardputer)) - Project author
- **AI Assistant** - Code development
- **Espressif** - ESP-IDF, USB Host Library, HID Host Driver
- **USB-IF** - HID Usage Tables specification

## License

MIT License. Use freely in your projects!
