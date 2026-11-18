# XIAO ESP32-S3 Sense – Wi-Fi most + kamera

Firmvér pre Seeed Studio XIAO ESP32-S3 Sense: SoftAP + WebSocket server, transparentný most WebSocket ↔ UART do STM32, MJPEG stream z kamery OV2640.

- `data/index.html` – webové UI (PC / telefón / Steam Deck), servované na `http://192.168.4.1/`
- WebSocket endpoint: `ws://192.168.4.1/ws`, binárne rámce podľa protokolu v [../README.md](../README.md)
- UI sa dá otvoriť aj lokálne v prehliadači (bez dronu ukazuje stav odpojený)
