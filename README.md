# BLE MESH Node

This repository provides an implementation for Bluetooth Low Energy (BLE) Mesh network nodes using ESP32. The project allows you to create BLE mesh nodes that can communicate with each other, enabling the development of smart home solutions or other IoT-based applications.

## Features
* BLE Mesh Communication: Implements BLE Mesh protocol on ESP32 for communication between nodes.
* ESP32 Compatibility: Works with ESP32 and compatible microcontrollers.
* Simple Setup: Easy-to-follow steps for setting up and configuring nodes.
* Scalable: Supports multiple nodes in the mesh network, ideal for smart home and IoT applications.

## Prerequisites

Before using this project, make sure you have:

* ESP32-C3 development board.
* ESP-IDF installed for compiling the firmware.
* A computer with Python installed for managing dependencies.

## Build

```sh
$ cd app
$ sh scripts/build.sh <procuct_name_string>
```
## Adding a Product to build

Modify `scripts/prod_profile.yml` to add another entry under the `products` object for the build.

```yml
- name: 4_relay_panel               # Product Name
    pid: 0x0001                     # Product ID
    elements:                       # Elements Used in Product
        - switch_relay_server: 4    # Element Name : No of instances of the element
```
## Adding Element, model to build:
Modify `scripts/model_profile.yml` to add another entry under the `products` object for the build.

## Flashing and monitoring

Once the above build is successful you can use the following to flash using `idf.py` tool

```sh
$ idf.py -p /dev/ttyACM0 flash monitor
```
