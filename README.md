# NMTV 1.54 – Duino-Coin Miner (PlatformIO)

 
**Stand: 16.09.2026**  
**Hardware-Basis:** ESP32- CYD + ST7789 320×240

Diese Version ist die aktuelle NMTV-Masterbasis für PlatformIO. Sie kombiniert den Duino-Coin-Miner mit einer kompakten 240×240-Anzeige, lokalem Web-Dashboard und zusätzlichen Diagnosefunktionen für den Dual-Core-Betrieb.

## Features

- Duino-Coin Mining auf beiden ESP32-Cores
- ST7789 240×240 Display mit kompakter Miner-Übersicht
- Anzeige von Hashrate, Difficulty, Shares und JOBS
- **JOBS-Zähler** als Aktivitätsanzeige (0–999999, danach Neustart bei 0)
- Automatisch kleinere Schrift bei langen JOBS-Werten
- WLAN-Status / Signalstärke auf dem Display
- Lokales Web-Dashboard mit Miner- und Diagnosewerten
- OTA-Unterstützung
- Native FreeRTOS-Tasks für den Dual-Core-Betrieb
- **Zwei-Türen-Collision-Guard:** jeder Core bevorzugt seinen eigenen Zugriffspfad und versucht bei Belegung den zweiten
- Kein enges Warten/Hämmern auf einen belegten Lock; sind beide belegt, wird der aktuelle Zugriff verworfen
- Kollisionsstatistik getrennt nach Core 0 / Core 1 inklusive Gesamt- und Prozentwerten
- **Crash-/Boot-Blackbox** mit Reset-Ursache und Checkpoints
- Speicherung von bis zu 10 relevanten Crash-Einträgen in NVS
- Erfassung von Panic-, Watchdog- und Brownout-Resets
- Crashlog im Web-Dashboard einsehbar und löschbar
- DSHA1-Warmup gegen Out-of-Bounds-Zugriff abgesichert
- Explizite Einbindung verwendeter Display-Fonts

## Anzeige

Die Hauptanzeige zeigt die wichtigsten Minerwerte direkt auf dem 240×240-Display. Das frühere Ping-/Best-Diff-Feld wird in dieser Master-Version als **JOBS** verwendet. Der Zähler beginnt nach einem Neustart bei 0 und zeigt, dass der Miner laufend neue Jobs empfängt und verarbeitet.

`BEST DIFF` wurde bewusst nicht übernommen: Der hier verwendete DUCO-Miningablauf sucht nach einem exakten SHA-1-Zielhash. Eine Bitcoin-artige „Best Difficulty“ aus beliebigen getesteten Nonces wäre daher keine aussagekräftige Kennzahl.

## Diagnose und Stabilität

Die Master-Version enthält zusätzliche Diagnosefunktionen, die während der Entwicklung der Dual-Core-Version entstanden sind. Die Crash-Blackbox merkt sich den letzten bekannten Programmzustand und kann nach einem relevanten Neustart Informationen dauerhaft ablegen. Die Zwei-Türen-Logik reduziert gleichzeitig Konflikte zwischen den beiden Mining-Tasks bei gemeinsam genutzten Ressourcen.

## Konfiguration

Die persönlichen Miner-, WLAN- und OTA-Einstellungen befinden sich in den entsprechenden Projekt-Konfigurationsdateien. Vor einer öffentlichen Veröffentlichung sollten private Zugangsdaten unbedingt entfernt bzw. durch Platzhalter ersetzt werden.

## PlatformIO

Das Projekt ist für PlatformIO aufgebaut. Die verwendete ESP32-/Arduino-Konfiguration befindet sich in `platformio.ini`.

## Versionsstand

**4.8 – Master / FIX9 JOBCOUNTER**

Die ausführliche Entwicklungshistorie und die technischen Änderungen stehen in `MASTER_CHANGELOG.txt`.

## Hinweis für weitere Geräte

Diese Version dient als Referenz für weitere ESP32-Varianten. Hardwareabhängige Teile wie Display-Pins, Touch, SPI, LDR oder RGB-LEDs müssen beim Übertragen an das jeweilige Gerät angepasst werden. Die NMTV-Pinbelegung sollte daher nicht blind auf andere Boards übernommen werden.
