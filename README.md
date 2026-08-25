# ESP32-CAM Autozähler

Lokale Bewegungsauswertung für ein AI-Thinker ESP32-CAM. Es werden keine Bilder
gespeichert oder an den Server übertragen; nur erkannte Zählereignisse werden
als JSON gepuffert und optional per HTTPS/HTTP-API gesendet.

## Bauen und Flashen

GitHub Actions baut `firmware.bin`. Für den Browser-Flasher wird zusätzlich eine
zusammengeführte Firmware benötigt. In PlatformIO lokal:

```sh
pio run
esptool.py --chip esp32 merge_bin -o merged-firmware.bin \
  0x1000 .pio/build/esp32cam/bootloader.bin \
  0x8000 .pio/build/esp32cam/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/esp32cam/firmware.bin
```

`merged-firmware.bin` wird bei Offset `0x0` geflasht. Nach dem ersten Start
steht im seriellen Monitor das einmalig erzeugte Gerätepasswort. Das WLAN
`AutoCounter-XXXXXX` wird mit genau diesem Passwort abgesichert.

## Online-Installer veröffentlichen

Das Repository nach GitHub pushen, unter **Settings → Pages** als Quelle
**GitHub Actions** auswählen und den Branch `main` pushen. Der Workflow
**Online-Installer veröffentlichen** baut die Firmware, führt sie zusammen und
veröffentlicht danach die GitHub-Pages-URL. Diese URL ist die komplette,
HTTPS-gesicherte Installationsseite.

## Kalibrierung

* Kamera seitlich, starr und mit enger ROI auf eine Fahrspur ausrichten.
* Im Dashboard die ROI sowie A/B-Linien setzen und Hintergrund neu aufbauen.
* `px_pro_meter` mit mehreren Fahrten eines Fahrzeugs bekannter Länge justieren.
* Eine Messung ist nur dann `sicher`, wenn beide Linien sauber passiert wurden.

Die Geschwindigkeitswerte sind nicht geeicht und ausschließlich für private
Auswertung bestimmt.
