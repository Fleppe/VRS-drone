#!/usr/bin/env bash
# Upravy vygenerovaneho Android projektu (spusta sa po `npx cap add android`)
set -euo pipefail
M=android/app/src/main/AndroidManifest.xml

# opravnenia, aby aplikacia vedela pouzit Wi-Fi DRON-VRS aj bez internetu
sed -i 's|<uses-permission android:name="android.permission.INTERNET" />|<uses-permission android:name="android.permission.INTERNET" />\n    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />\n    <uses-permission android:name="android.permission.CHANGE_NETWORK_STATE" />|' "$M"
grep -q CHANGE_NETWORK_STATE "$M"

# obrazovka iba na sirku
sed -i 's|android:name=".MainActivity"|android:name=".MainActivity"\n            android:screenOrientation="sensorLandscape"|' "$M"
grep -q sensorLandscape "$M"

# vlastna MainActivity (Wi-Fi bez internetu + cela obrazovka)
cp native/MainActivity.java android/app/src/main/java/sk/vrs/dron/MainActivity.java
echo "Android projekt upraveny"
