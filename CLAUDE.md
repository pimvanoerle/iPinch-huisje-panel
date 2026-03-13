# CLAUDE.md

## Project
**iPinch-huisje-panel** — a touch control panel for the iPinch huisje, built on the Elecrow CrowPanel Advance 7" V3.0 (DIS08070H). Integrates with the iPinch API and Home Assistant.

## Hardware: CrowPanel Advance 7" V3.0 (DIS08070H)

ESP32-S3, 800×480 RGB display, capacitive touch (GT911), GPIO19/20 I2C bus.

**Board settings (Arduino IDE):**
- Board: `ESP32S3 Dev Module`
- PSRAM: `OPI PSRAM`
- Partition: `huge_app`
- Upload speed: `460800` (921600 is unreliable on this board)

**Libraries:**
- LVGL 8.3.3
- LovyanGFX 1.2.19+

**Backlight:** `ledcAttach(2, 300, 8); ledcWrite(2, 255);`

### GT911 Touch Controller
- I2C address: **0x5D** (not 0x14 — board pull-up on INT selects 0x5D)
- SDA=GPIO19, SCL=GPIO20, configured as `I2C_NUM_1` in LovyanGFX (`pin_rst=-1`, `pin_int=-1`)

### PCA9557 reset sequence (required before `gfx.init()`)
PCA9557 GPIO expander at I2C 0x18 controls GT911 RST/INT.
IO0=RST, IO1=INT. **IO2-IO7 must stay HIGH** — they power other board circuits.

```cpp
pinMode(38, OUTPUT); digitalWrite(38, LOW);  // must be first
Wire.begin(19, 20);
pca_write(0x03, 0x00);  // all pins → output
pca_write(0x01, 0xFC);  // IO0=LOW(RST), IO1=LOW(INT), IO2-IO7=HIGH
delay(20);
pca_write(0x01, 0xFD);  // IO0=HIGH (RST released), IO1=LOW, IO2-IO7=HIGH
delay(100);
pca_write(0x03, 0x02);  // IO1 → input
Wire.end();             // release GPIO19/20 before gfx.init() claims I2C_NUM_1
delay(50);
gfx.init();
```

See https://github.com/pimvanoerle/CrowPanel for the reference driver files
(`test_app/CrowPanel_Test/LovyanGFX_Driver.h` and `pins_config.h`).

## Project Structure

```
panel/                  — Arduino sketch
  panel.ino
  secrets.h             — gitignored, copy from secrets.h.example
  secrets.h.example     — committed template with placeholder values
  LovyanGFX_Driver.h    — display + touch driver (from CrowPanel reference repo)
  pins_config.h         — LCD_H_RES / LCD_V_RES defines
```

## Secrets
All credentials live in `panel/secrets.h` (gitignored). Never hardcode or commit:
- `WIFI_SSID` / `WIFI_PASSWORD`
- `IPINCH_API_URL` / `IPINCH_API_KEY`
- `HA_URL` / `HA_TOKEN` (Home Assistant)
