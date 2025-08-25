# MeshX

```txt
*********************************************************************************************************************
* MMMMMMMM               MMMMMMMM                                     hhhhhhh                 XXXXXXX       XXXXXXX *
* M:::::::M             M:::::::M                                     h:::::h                 X:::::X       X:::::X *
* M::::::::M           M::::::::M                                     h:::::h                 X:::::X       X:::::X *
* M:::::::::M         M:::::::::M                                     h:::::h                 X::::::X      X:::::X *
* M::::::::::M       M::::::::::M    eeeeeeeeeeee        ssssssssss   h:::: hhhhhh            XX:::::X     X:::::XX *
* M:::::::::::M     M:::::::::::M  ee::::::::::::ee    ss::::::::::s  h::::::::::hhh            X:::::X   X:::::X   *
* M:::::::M::::M   M::::M:::::::M e::::::eeeee:::::eess:::::::::::::s h::::::::::::::hh           X:::::X:::::X     *
* M::::::M M::::M M::::M M::::::Me::::::e     e:::::es::::::ssss:::::sh:::::::hhh::::::h           X:::::::::X      *
* M::::::M  M::::M::::M  M::::::Me:::::::eeeee::::::e s:::::s  ssssss h::::::h   h::::::h          X:::::::::X      *
* M::::::M   M:::::::M   M::::::Me:::::::::::::::::e    s::::::s      h:::::h     h:::::h         X:::::X:::::X     *
* M::::::M    M:::::M    M::::::Me::::::eeeeeeeeeee        s::::::s   h:::::h     h:::::h        X:::::X X:::::X    *
* M::::::M     MMMMM     M::::::Me:::::::e           ssssss   s:::::s h:::::h     h:::::h     XXX:::::X   X:::::XXX *
* M::::::M               M::::::Me::::::::e          s:::::ssss::::::sh:::::h     h:::::h     X::::::X     X::::::X *
* M::::::M               M::::::M e::::::::eeeeeeee  s::::::::::::::s h:::::h     h:::::h     X:::::X       X:::::X *
* M::::::M               M::::::M  ee:::::::::::::e   s:::::::::::ss  h:::::h     h:::::h     X:::::X       X:::::X *
* MMMMMMMM               MMMMMMMM    eeeeeeeeeeeeee    sssssssssss    hhhhhhh     hhhhhhh     XXXXXXX       XXXXXXX *
*********************************************************************************************************************
```
[![CI Pipeline:main](https://github.com/pranjalchanda08/MeshX/actions/workflows/ci.yml/badge.svg)](https://github.com/pranjalchanda08/MeshX/actions/workflows/ci.yml) ![Static Badge](https://img.shields.io/badge/Release-v0.2-blue?link=https%3A%2F%2Fgithub.com%2Fpranjalchanda08%2FMeshX%2Freleases%2Ftag%2Fv0.2)


This repository provides an implementation for Bluetooth Low Energy (BLE) Mesh network nodes using ESP32. The project allows you to create BLE mesh nodes that can communicate with each other, enabling the development of smart home solutions or other IoT-based applications.

## Features
* BLE Mesh Communication: Implements BLE Mesh protocol on ESP32 for communication between nodes.
* ESP32 Compatibility: Works with ESP32 and compatible microcontrollers.
* Simple Setup: Easy-to-follow steps for setting up and configuring nodes.
* Scalable: Supports multiple nodes in the mesh network, ideal for smart home and IoT applications.

## Capabilities

### Server

| Element           | Models                |
| ----------------- | --------------------- |
| Relay Server      | Generic On Off Server |
| Light CWWW Server | Generic On Off Server |
|                   | Light CTL Server      |

### Client

| Element           | Models                |
| ----------------- | --------------------- |
| Relay Client      | Generic On Off Client |
| Light CWWW Client | Generic On Off Client |
|                   | Light CTL Client      |

## Prerequisites

Before using this project, make sure you have:

* ESP32-C3 development board.
* ESP-IDF installed for compiling the firmware.
* A computer with Python installed for managing dependencies.
* Install docker: [docker-ubuntu](https://docs.docker.com/engine/install/ubuntu/)
* Python: 3.12+

## Build

Source the terminal
```sh
$ export IDF_PATH=</idf_path>
$ source $IDF_PATH/export.sh
```

Linux
```sh
$ cd app
$ sh scripts/build.sh <product_name_string>
```

Windows
```sh
$ cd app
$ .\scripts\build.bat <product_name_string>
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
