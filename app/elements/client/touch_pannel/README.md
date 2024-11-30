# Touch Pannel

Touch Pannel is a control node that would be used by the user to control other devices.

## Node HW Config
1. Max allowed Touch elements - 8
2. 1 multiplexed analog controller available via rotary encoder -- GPIO
3. Touch sensor interface -- I2C + GPIO(INTR) -- CAP1118 / TTP224
4. OLED monochrome Display -- I2C

## Element possible profiles

1. Light (Relay)
2. Light (CWWW)
3. Light (Hue)
4. Fan (Relay)
5. Fan (IR)
6. Curtain
7. AC appliance (Relay)

## Capabilities
1. Touch swich with multi mode selectivty
2. 1 rotary digital dimmer for controling secondary modes.
3. Gateway capability to translate data over Wi-Fi <--> BLE Mesh
4. Universal IR blaster for controling remote control based appliances.
5. OTA updates
