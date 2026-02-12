# IEEE 802.15.4 Transceiver

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/reference/en/libraries/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)



This library provides an high level API for [IEEE 802.15.4 ](https://en.wikipedia.org/wiki/IEEE_802.15.4)communication using the ESP32's built-in radio. It supports initializing the transceiver, transmitting and receiving frames, setting receive callbacks, and dynamically switching channels (11-26) in promiscuous mode.

This library was inspired by shoderico's [esp_ieee802154-transceiver](https://components.espressif.com/components/shoderico/ieee802154_transceiver/versions/1.0.0/readme) ESP-IDF component, extended and refactured for C++.

## Features

- Initialize the IEEE 802.15.4 radio in promiscuous or filtering mode for flexible frame capture.
- Transmit and receive IEEE 802.15.4 frames with support for custom frame structures.
- Register callbacks to process received frames with RSSI and LQI information.
- Dynamically switch channels (11-26) without reinitializing the radio.
- Arduino Stream integration

## Requirements

- ESP32 with IEEE 802.15.4 support (e.g., ESP32-C6, ESP32-H2).

## Documentation

- Class Documentation
  - ESP32TransceiverIEEE802_15_4 
  - ESP32TransceiverStream

- ESP API used by this project
  - [ESP IDF IEEE802.15.4 API](https://github.com/espressif/esp-idf/blob/master/components/ieee802154/include/esp_ieee802154.h)

- Examples
  - [bridge](examples/bridge/bridge.ino)
  - [sniffer](examples/sniffer/sniffer.ino)
  - [transceiver](examples/transceiver/transceiver.ino)

## Installation in Arduino

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/ESP32TransceiverIEE802_15_4.git
```

I recommend to use git because you can easily update to the latest version just by executing the ```git pull``` command in the project folder.

## License

Licensed under the [MIT](LICENSE).

## Contributing

Contributions are welcome! Submit issues or pull requests to the repository at [your repository URL].

