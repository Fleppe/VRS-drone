# ESP32 – Wi-Fi most

Firmvér pre klasické ESP32: Wi-Fi AP + webové UI + WebSocket ↔ UART most do STM32.

- `esp32_bridge/esp32_bridge.ino` – sketch pre Arduino IDE (knižnica **WebSockets** od Markus Sattler)
- `esp32_bridge/index_html.h` – UI vložené do firmvéru (generované, needitovať)
- `data/index.html` – zdrojový kód UI, dá sa otvoriť aj priamo v prehliadači
- `html_to_header.py` – po zmene UI: `python html_to_header.py`

Návod na spustenie: [../README.md](../README.md#firmvér-esp32--ako-to-spustiť)
