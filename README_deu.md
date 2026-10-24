<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | 🇩🇪 <b>Deutsch</b> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Ein ESP32-S3-Knoten, der im Gehäuse einer Bungard-CCD-Maschine sitzt, am RS-232-Anschluss ihrer Steuereinheit hängt und die EH3D-Programme die Maschine über Ethernet oder WLAN steuern lässt, mit den Lampen der Maschine und dem Antrieb der Spindel auf derselben Platine

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Ehrlichkeitscheck - was heute tatsächlich läuft:** eine C++17-Firmware für den ESP32-S3 (ESP-IDF 5.5), die den RS-232-Anschluss der Steuereinheit einer Bungard-CCD-Maschine zu einem Netzwerkanschluss macht: Ein Programm verbindet sich per TCP mit einem Zugriffstoken, schaltet die Maschine scharf und sendet die Bytes einer Bewegung, die der Knoten einreiht und im Takt der Leitung ausgibt. Er liest außerdem die Lampen der Maschine (HOME X, Y und Z, ENABLE, TOOL), steuert eine Spindel über ein 0-10-V-Drehzahlsignal und Lauf-/Richtungsleitungen und hat ein Web-Panel (acht Sprachen) für Einrichtung, Benutzer, Netzwerk, Update und die eigenen Einstellungen der Maschine. Die Regeln der Verbindung und die Einstellungen werden auf einem Rechner getestet, und die Firmware baut für beide Platinen ohne Warnungen. **Sie wurde noch nicht auf eine Platine gespielt oder dort ausgeführt**, und die Trägerplatine mit RS-232-Wandler, Lampeneingängen und Spindelantrieb ist ein Entwurf auf dem Papier.

---

> **Stand von Sicherheit und IT-Sicherheit:** dies ist kein Sicherheitssystem. Ein falsch verdrahteter Knoten oder ein falsch eingestellter Spindelantrieb kann eine Maschine bewegen oder ein Werkzeug drehen lassen. Der Knoten verweigert die Steuerung ohne Zugriffstoken, nimmt Bytes für die Maschine nur an, solange ein Programm ihn scharf geschaltet hat, und hält an (Warteschlange geleert, Schrittmotortreiber und Spindel aus), wenn die Sitzung verstummt oder die Verbindung abbricht (außer die optionale Einstellung, die ihn den gehaltenen Auftrag beenden lässt, ist an). Die Endschalter der Maschine sieht er nur über die Lampen, und einen eigenen Not-Aus hat er noch nicht (die Trägerplatine hat Platz für einen Tastereingang). Der Spindelantrieb liegt an Netzspannung: verdrahten und einstellen nach dem Handbuch des Herstellers. Das Panel bewegt die Maschine nie und speichert keine Einstellungen, solange ein Programm die Maschine scharf hält. Das Projekt ist unabhängig: Es wird nicht von Bungard Elektronik hergestellt, empfohlen oder unterstützt, und Produktnamen erscheinen nur, um zu sagen, mit welchen Maschinen und Antrieben es arbeiten soll.

## 📋 Auf einen Blick

| | |
|---|---|
| Version | `0.0.2` |
| Reifegrad | Gerüst |
| Tests | 9 Host-Testsuiten (etwa 2200 Prüfungen) |
| Technologie-Stack | C++17, ESP-IDF 5.5, ESP32-S3 |
| Auf echter Hardware | Nein: beide Firmware-Images bauen, aber noch nichts lief auf einer Platine |

## 🚦 Projektstatus

Jede Zeile ist ein Teil dieses Repositorys, und die Ampel sagt, wie weit er heute ist. Sie beruht auf dem, was das Repository zeigen kann (Tests, Builds, Läufe), nicht auf dem, was geplant ist.

🟢 Fertig und überprüft · 🟡 Funktioniert, mit bekannten Grenzen · 🟠 Teilweise fertig oder noch nicht überprüft · 🔴 Nicht gemacht oder funktioniert nicht

| Bereich | Stand | Was das hier bedeutet |
|---|---|---|
| Software | 🟡 | Die Firmware für den ESP32-S3 (ESP-IDF 5.5, C++17) baut ohne Warnungen für beide Platinen, Ethernet und WLAN; kein Image wurde geflasht. |
| Tests | 🟢 | 9 Testprogramme auf dem PC bestehen (`ctest`), rund 2.200 Prüfungen: die Verbindung, die Einstellungen, die Maschinenregeln, die gemeinsame Basis, die Panel-Texte und die Task-Sicherheit. |
| Steuerverbindung | 🟡 | Das Rahmenprotokoll (CRC, Token-Anmeldung, Scharfschalten, Byte-Warteschlange, Stopp bei stiller Sitzung) ist durch die Host-Tests abgedeckt; kein Programm hat mit einem echten Knoten gesprochen. |
| Web-Panel | 🟡 | Das Web-Panel (acht Sprachen: Einrichtung, Benutzer, Netzwerk, Einstellungen, Update) hat geprüfte Texte und wird gegen eine Attrappe des Knotens erprobt; es wurde nie von einer Platine ausgeliefert. |
| Spindel und Lampen | 🟠 | Das 0-10-V-Drehzahlsignal, die Lauf- und Richtungsleitungen (keine Umkehr während der Drehung) und die fünf Lampeneingänge sind am Host getestet; weder ein Spindelantrieb noch eine Maschine wurde angeschlossen. |
| Updates | 🟠 | Der Upload in einen von zwei Firmware-Slots mit Rollback und Bestätigung ist am Host getestet; ein Update lief nie auf einer Platine. |
| Sicherheit | 🟡 | Einrichtungscode auf der USB-Konsole, Zugriffstoken, Benutzer und Rollen, optionales HTTPS und nichts Geheimes im Image; er hält von selbst an, wenn die Verbindung abreißt. Es gibt noch keinen Not-Halt-Eingang und keine unabhängige Prüfung. |
| Echte Hardware | 🔴 | Keine Platine hat diese Firmware ausgeführt, und die Trägerplatine mit dem RS-232-Wandler, den Lampeneingängen und dem Spindelantrieb ist nur ein Entwurf auf Papier. |
| Dokumentation | 🟢 | README in acht Sprachen, das Protokoll, die Firmware, die Verdrahtung und der Entwurf der Trägerplatine. |

## 🎯 Überblick

* Die **Steuerverbindung** (TCP, Standardport 5800): ein Protokoll mit Rahmen und CRC, eine Anmeldung mit Zugriffstoken, das Scharfschalten, eine Warteschlange von Bytes für die Steuereinheit und ein Stopp, wenn die Sitzung verstummt oder abbricht. Das Programm sendet die Bytes einer Bewegung bereits getaktet, und die UART des Knotens legt sie exakt auf die Leitung, was ein USB-Seriell-Adapter bei hoher Geschwindigkeit nicht schafft.
* Die **serielle Leitung zur Steuereinheit**: 38400 Baud 8N1 über einen MAX3232, mit den Leitungen DTR, DCD und RI des Handshakes. Die Pins und ob der Wandler die Leitungen invertiert, sind Einstellungen.
* Die **Lampen**: HOME X, HOME Y, HOME Z, ENABLE und TOOL werden über einen Pufferttransistor gelesen, im Status und im Panel angezeigt, und eine Anfrage kann den Knoten den Rest einer Suche verwerfen lassen, sobald eine Achse in Referenz ist, sodass das Programm die Home-Leitung nicht mehr mitten im Datenstrom lesen muss.
* Die **Spindel**: ein Drehzahl-PWM für einen 0-10-V-Wandler (standardmäßig 24000 U/min bei vollem Signal, für einen Frequenzumrichter wie den Huanyang HY02D223B) und Lauf-vorwärts-/rückwärts-Leitungen, ohne Richtungswechsel während sie dreht oder gleich nach dem Stopp, und mit Stopp, sobald die Maschine angehalten wird.
* Das **Panel** (HTTP und HTTPS, acht Sprachen): Ersteinrichtung mit einem Code auf der USB-Konsole, Benutzer, Ethernet oder WLAN, die Steuerverbindung, die RS-232-Pins, die Lampen und die Spindel, ein Log und Updates per Upload in einen von zwei Firmware-Slots mit Rollback. Nichts Geheimes wird ins Image einkompiliert.
* Der **Selbstschutz**: eine Wache über Tasks und Speicher, ein Tor vor dem Panel bei knappem Speicher und die Bestätigung eines neuen Images, sobald der Knoten erreichbar ist.
* Die **Platinen**: das Waveshare ESP32-S3-ETH (W5500-Ethernet) und ein ESP32-S3-WROOM-1 N16R8 (WLAN), je ein Image. Der Entwurf der Trägerplatine (100 x 100 mm) steht in `docs/BOARD.md`.
* Die **Maschinen**: die Bungard CCD MTC und CCD/2 MTC mit der Steuereinheit an der seriellen Schnittstelle der DOS-Software 4.6. Die beiden ATC-Modelle sind nicht abgedeckt; die XL-Modelle und spätere Maschinen haben eine eigene moderne Steuereinheit.
* Das **selbstständige Beenden** (standardmäßig aus, eine Einstellung): Wenn das Programm oder der Server wegfällt, führt der Knoten die Bytes in seiner Warteschlange bis zum Ende aus - einschließlich der darin eingereihten Spindelbefehle - und schaltet dann Spindel und Treiber ab. Die Warteschlange liegt im externen RAM (bis 4 MB, Minuten an Bewegung); ein Zeitlimit beendet es, ebenso ein Abort und der Stopp-Knopf des Panels. **Geplant, noch nicht gebaut**: ein Tastereingang für einen physischen Stopp.

## 🧩 Rolle in EH3D

EH3D-NODE-BUNGARD ist die letzte Stufe zwischen den EH3D-Programmen und der Maschine: EH3D-DRILLPRO und EH3D-ROUTEPRO verbinden sich heute damit, und EH3D-SERVER wird davor sitzen. Er ist der erste Knoten der EH3D-Familie, und seine Einstellungen, sein Panel und das Update-System sollen mit den folgenden Knoten geteilt werden (die gemeinsame Basis liegt in EH3D-COMMON).

## 📂 Repository-Struktur

```text
EH3D-NODE-BUNGARD/
├── core/         Rahmen und Sitzungsregeln der Verbindung, die Byte-Warteschlange und die Einstellungen: ohne Hardware, auf einem Rechner getestet
├── main/         die ESP-IDF-Firmware: UART, Steuerverbindung, Lampen, Spindel, Netzwerk, Einstellungsspeicher, Webserver
├── panel/        das Web-Panel, in die Firmware gepackt
├── tests/        Host-Tests und Prüfungen der Panel-Texte
├── tools/        build_node.sh, pack_panel.py, check_sdkconfig.py, ci_validate.py, panel_mock.mjs
├── CMakeLists.txt, partitions.csv, sdkconfig.defaults, sdkconfig.board.*  das ESP-IDF-Projekt, die Flash-Aufteilung und die Einstellungen jeder Platine (s3-eth, s3-wifi)
├── docs/         PROTOCOL, NODE_FIRMWARE, WIRING und BOARD
├── images/       Banner
└── eh3d.project.json
```

## 🛠️ BAUEN UND AUSFÜHREN

Für die Host-Tests brauchst du einen C++17-Compiler und CMake, für die Firmware ESP-IDF 5.5 (oder Docker).

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth` (oder `s3-wifi`) baut ein Image im ESP-IDF-Container; unter Windows tut ein installiertes ESP-IDF 5.5.5 dasselbe. Das Image `dist/eh3d-node-bungard-<Version>-<Platine>-complete.bin` gilt für EINE Platine: Spiele es an Adresse 0 mit `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <Image>` auf. Dann verbinde dich mit dem WLAN `EH3D-SETUP-xxxxxx`, das der Knoten öffnet, rufe `http://192.168.4.1/` auf und gib den Einrichtungscode ein, den der Knoten auf seiner USB-Konsole zeigt; Zugriffstoken, Netzwerk und Pins werden im Panel eingestellt.

## 🔗 Verwandte Projekte

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): das Bohrprogramm; es steuert die Maschine über diesen Knoten mit `connection = node`
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): das Fräsprogramm; es teilt die Protokollbibliothek und die Maschinendatei
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): die gemeinsamen Teile der EH3D-Familie; die Basis der Knoten-Firmware liegt dort
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): das Zentrum der EH3D-Familie: Er führt die Aufträge der Programme und der anderen Clients über diesen Knoten auf der Maschine aus

## 📚 Dokumentation und Community

* [Die Steuerverbindung](docs/PROTOCOL.md): Rahmen, Anmeldung, Scharfschalten, Warteschlange, Spindel, Lampen und die Regeln, die die Maschine stoppen.
* [Die Firmware](docs/NODE_FIRMWARE.md): Einrichtung, Einstellungen, Panel, Update mit Rollback, Speicher und Selbstschutz.
* [Verdrahtung](docs/WIRING.md): der RS-232-Anschluss, die Lampen und der Spindelantrieb.
* [Die Trägerplatine](docs/BOARD.md): der Entwurf, aus dem der Schaltplan gezeichnet wird.
* [Änderungsprotokoll](CHANGELOG.md)
* [Mitwirken](CONTRIBUTING.md)
* [Sicherheitsrichtlinie](SECURITY.md)
* [Support](SUPPORT.md)

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LIZENZ

Software: GPL-3.0-or-later. Siehe [LICENSE](LICENSE). Die Hardware-Entwürfe der Suite verwenden die CERN OHL-S.
