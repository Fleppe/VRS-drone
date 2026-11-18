# STM32 – komunikačný modul

Firmvér na STM32F303K8: UART príjem/odosielanie, parser rámcov, CRC8, telemetria, failsafe.

- `comm.c` / `comm.h` – UART DMA príjem, parser, odosielanie rámcov
- `protocol.h` – definície rámcov (TYPE, štruktúry payloadu)
