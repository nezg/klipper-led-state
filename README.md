# Klipper LED State

**Klipper LED State** is a simple project for creating a beautiful and customizable **LED status bar for a Klipper-controlled 3D printer**, inspired by H2D. It displays the printer status, print progress, bed temperature, and lighting effects such as **breath** and **wave**.

The project works **out-of-the-box**, with **no changes required to the printer configuration**. All effects and LED control are processed on a **low-cost ESP32 board**, ensuring **minimal implementation cost**. Firmware receives printer status over **Wi-Fi** via **WebSocket** (supports Klipper/Moonraker).  

---

## Supported Boards

- **ESP32** (classic)  
- **ESP32-C3**  
- **ESP32-S3**  

Firmware can be compiled in **Arduino IDE** for all supported boards.  

---

## Features

- Low-cost solution: all effects processing is done on **ESP32**, no additional controllers needed.  
- Supports **any 5V addressable LED strips** (WS2812/NeoPixel and compatible).  
- Automatic printer status detection over the local network.  
- Configurable **Data Pin**, LED type, and number of LEDs.  
- Configurable **colors and effects** with live preview (selected color shown on LEDs for 2 seconds in Web interface):  
  - Breath — global effect  
  - Wave — for specific statuses, e.g., pause  
- Displays:  
  - Current print status in two configurable colors with smooth gradient  
  - Bed temperature for **standby** and **complete** as a temperature scale on LED edges  
  - Printer state with separate colors for **standby**, **pause**, **complete**, and **error/offline**  
- Supports **OTA updates**  
- No printer configuration editing required  

---

## Assembly

Minimum required components:  

- **ESP32**, **ESP32-C3**, or **ESP32-S3** board  
- Any 5V addressable LED strip (WS2812/NeoPixel compatible)  
- Power supply for board and LED strip (5V, current depends on strip length, e.g., DC-DC 24V → 5V)  
- Connecting wires  

### Recommended components for stability

- 1000µF 6.3V+ capacitor between +5V and GND of the strip  
- 220–470Ω resistor on DATA line  

> Capacitor optional for short strips; recommended for longer LED strips.  

### Wiring example (WS2812)

ESP32 → WS2812 LED Strip  
GND → GND  
5V → +5V  
GPIOXX → Data In (GPIO04, GPIO05, GPIO01, GPIO02 supported, configurable in firmware)  

- **Always connect ESP32 GND to LED strip GND**  

---

## Firmware and Setup

### Compilation (optional, see Releases section)

1. Open the project in **Arduino IDE**  
2. Select your board:  
   - **ESP32 Dev Module**  
   - **ESP32C3 Dev Module**  
   - **ESP32S3 Dev Module**  
3. Set upload speed: 115200  
4. Compile project (`Ctrl+R` or Verify/Compile button)  

### Flashing Firmware

- **Precompiled binaries:**  
  Download `_full.bin` from Releases and flash via browser using:  
  [https://esptool.spacehuhn.com/](https://esptool.spacehuhn.com/)  
  - Address: **0x00**  

- **Via USB:**  
  1. Connect ESP32 to PC  
  2. Click “Upload” in Arduino IDE  

- **Via OTA (Wi-Fi updates):**  
  1. Use `_OTA.bin` from Releases for your board  
  2. Connect via Web interface → choose firmware → Update Firmware  

### Wi-Fi Setup

After flashing, ESP32 creates a Wi-Fi hotspot:  

**Klipper_State_LED_Config**  
Password: `12345678`  

1. Connect from your phone or computer  
2. Go to **192.168.4.1**  
3. In **Wi-Fi** section:  
   - Set your Wi-Fi network and password  
   - Or leave as is and connect printer directly  
4. In **Printer Settings**, set printer IP and refresh page  
- Once connected, ESP32 receives printer status via WebSocket  

### LED Strip Settings (Web interface)

- LED Config:  
  - Data Pin  
  - Number of LEDs  
  - Status direction (left→right or right→left)  
- Color Config:  
  - Global brightness  
  - Colors for printer states  
  - Breath and Wave effects  
  - Max bed temperature scale length  

---

## Release Structure

| Board | Full binary | OTA binary |
|-------|------------|------------|
| ESP32  | `ESP32_full.bin` | `ESP32_ota.bin` |
| ESP32-C3 | `ESP32C3_full.bin` | `ESP32C3_ota.bin` |
| ESP32-S3 | `ESP32S3_full.bin` | `ESP32S3_ota.bin` |

---

## Contribution and Improvements

All contributions welcome:  

- New effects and animations  
- Support for additional boards and LEDs  
- Web interface enhancements  
- Code optimization  

Create **Pull Request** or **Issue** on GitHub.  

---

## License

MIT License — free use, modification, and redistribution  

---

# Klipper LED State (Русский перевод)

**Klipper LED State** — это простой проект для создания красивой настраиваемой **LED-статусной строки для 3D-принтера, управляемого Klipper**, вдохновлённой H2D. Она отображает статус принтера, процесс печати, температуру стола и эффекты свечения: **breath** и **wave**.

Проект **работает «из коробки»**, без изменения конфигурации принтера. Вся обработка эффектов выполняется на **дешёвой плате ESP32**, что делает реализацию **недорогой**. Прошивка получает статус принтера по **Wi-Fi** через **WebSocket**.  

---

## Поддерживаемые платы

- **ESP32** (классический)  
- **ESP32-C3**  
- **ESP32-S3**  

Прошивка компилируется в **Arduino IDE**.  

---

## Особенности

- Низкая себестоимость — эффекты выполняются на **ESP32**  
- Поддержка **любых адресных лент 5В** (WS2812/NeoPixel)  
- Автоматическое получение статуса принтера  
- Настраиваемые **Data Pin**, тип ленты и количество светодиодов  
- Настраиваемые **цвета и эффекты** с предпросмотром  
- Отображение: статус печати, температура стола, состояние принтера  
- Поддержка OTA  
- Не требует редактирования конфигурации принтера  

---

## Сборка

Компоненты: ESP32/ESP32-C3/ESP32-S3, лента 5В, питание, провода  

Рекомендуется: конденсатор 1000µF 6.3V+, резистор 220–470Ω на DATA  

Схема подключения: GND → GND, 5V → +5V, GPIOXX → DATA  

---

## Прошивка и настройка

### Компиляция

Открыть проект в Arduino IDE, выбрать плату, скорость 115200, компилировать.  

### Загрузка

- **Готовые бинарники:** скачать `_full.bin` из релизов и прошить через [esptool.spacehuhn.com](https://esptool.spacehuhn.com/) (адрес 0x00)  
- **Через USB:** «Upload» в Arduino IDE  
- **Через OTA:** `_OTA.bin`, Web-интерфейс → Update Firmware  

### Настройка Wi-Fi

Точка доступа: **Klipper_State_LED_Config**, пароль `12345678`  
Подключиться, перейти на 192.168.4.1, указать Wi-Fi или оставить точку доступа, IP принтера в Printer Settings.  

### Настройка ленты

LED Config: пин, количество, направление  
Color Config: яркость, цвета, эффекты, длина температурной шкалы  

---

## Структура релиза

| Плата | Полный бинарник | OTA бинарник |
|-------|----------------|--------------|
| ESP32  | `ESP32_full.bin` | `ESP32_ota.bin` |
| ESP32-C3 | `ESP32C3_full.bin` | `ESP32C3_ota.bin` |
| ESP32-S3 | `ESP32S3_full.bin` | `ESP32S3_ota.bin` |

---

## Вклад

Любые дополнения приветствуются: эффекты, платы, Web-интерфейс, оптимизация. Pull Request или Issue на GitHub.  

---

## Лицензия

MIT — свободное использование, модификация и распространение.
