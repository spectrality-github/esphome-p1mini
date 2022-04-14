# esphome-p1mini
Based on esphome-p1reader, which is an ESPHome custom component for reading P1 data from electricity meters. Designed for Swedish meters that implements the specification defined in the [Swedish Energy Industry Recommendation For Customer Interfaces](https://www.energiforetagen.se/forlag/elnat/branschrekommendation-for-lokalt-kundgranssnitt-for-elmatare/) version 1.3 and above.

Notable differences from esphome-p1reader are:
* More frequent updates with configurable update period.
* No additional components needed. RJ12 cable connects directly to D1Mini (or equivalent)

## ESPHome version
The current version in main is tested with ESPHome version `2022.3.0`. Make sure your ESPHome version is up to date if you experience compile problems.

## Verified meter hardware / supplier
* [Sagemcom T211](https://www.ellevio.se/globalassets/uploads/2020/nya-elmatare/ellevio_produktblad_fas3_t211_web2.pdf) / Ellevio

## Meters verified with esphome-p1reader, which should work too...
* [Landis+Gyr E360](https://eu.landisgyr.com/blog-se/e360-en-smart-matare-som-optimerarden-totala-agandekostnaden)
* [Itron A300](https://boraselnat.se/elnat/elmatarbyte-2020-2021/sa-har-fungerar-din-nya-elmatare/) / Borås Elnät
* [S34U18 (Sanxing SX631)](https://www.vattenfalleldistribution.se/matarbyte/nya-elmataren/) / Vattenfall 
* [KAIFA MA304H4E](https://reko.nackaenergi.se/elmatarbyte/) / Nacka Energi
* [KAIFA CL109](https://www.oresundskraft.se/dags-for-matarbyte/) / Öresundskraft

## Hardware
I have used a D1 mini clone, but most ESP-based controllers should work as long as you figure out appropriate pins to use. The P1 port on the meter provides 5V up to 250mA which makes it possible to power the circuit directly from the P1 port.

### Parts
- 1 (Wemos) D1 mini or clone.
- 1 RJ12 cable (6 wires)
- Optionally, hot melt glue and large heat shrink tubing.

### Wiring
The circuit is very simple, basically the 5V TX output on the P1 connector is converted to 3.3V and inverted by the transistor and connected to the UART0 RX pin on the microcontroller. The RTS (request to send) pin is pulled high so that data is sent continously and GND and 5V is taken from the P1 connector to drive the microcontroller.

#### Wiring NodeMCU ESP-12
![Wiring Diagram](images/wiring.png)


## Installation
Clone the repository and create a companion `secrets.yaml` file with the following fields:
```
wifi_ssid: <your wifi SSID>
wifi_password: <your wifi password>
p1mini_password: <Your p1mini password (for OTA, API, etc)>
```
Make sure to place the `secrets.yaml` file in the root path of the cloned project. The `p1mini_password` field can be set to any password before doing the initial upload of the firmware.

Flash ESPHome as usual, just keep the `p1mini.h` file in the same location as `p1mini.yaml` (and `secrets.yaml`). *Don't* connect USB and the P1 port at the same time! If everything works, Home Assistant will autodetect the new integration when you plug it into the P1 port.

If you do not receive any data, make sure that the P1 port is enabled and try setting the log level to `DEBUG` for more feedback.

## Technical documentation
Specification overview:
https://www.tekniskaverken.se/siteassets/tekniska-verken/elnat/aidonfd-rj12-han-interface-se-v13a.cleaned.pdf

OBIS codes:
https://tech.enectiva.cz/en/installation-instructions/others/obis-codes-meaning/

P1 hardware info (in Dutch):
http://domoticx.com/p1-poort-slimme-meter-hardware/
