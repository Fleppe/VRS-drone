# Android aplikácia

Rovnaké UI ako na ESP32 (`../esp32-bridge/data/index.html`), zabalené cez Capacitor do APK.

Oproti prehliadaču:
- celá obrazovka a orientácia na šírku natrvalo,
- všetka komunikácia ide cez Wi-Fi `DRON-VRS`, aj keď telefón hlási „bez internetu“,
- UI je uložené v aplikácii a pripája sa na `ws://192.168.4.1:81/`.

## Ako získať APK
APK sa zostaví automaticky cez **GitHub Actions** (`.github/workflows/android-apk.yml`) pri každej zmene UI alebo tohto priečinka.

1. GitHub → záložka **Actions** → **Android APK** → posledný úspešný beh.
2. Dole v časti **Artifacts** stiahnuť **dron-vrs-apk** (zip s `app-debug.apk`).
3. APK skopírovať do telefónu a nainštalovať (povoliť inštaláciu z neznámych zdrojov).

## Súbory
- `package.json`, `capacitor.config.json` – Capacitor projekt
- `native/MainActivity.java` – Wi-Fi bez internetu + celá obrazovka
- `patch-android.sh` – úpravy vygenerovaného Android projektu (oprávnenia, orientácia)
- `android/`, `www/`, `node_modules/` sa negenerujú do gitu, vytvorí ich build
