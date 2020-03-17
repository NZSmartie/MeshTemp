# MeshTemp

Bringing Bluetooth mesh to Xiaomi's Bluetooth Temperature Sensors

## Notes

- This project is still in development. The Xiaomi Bluetooth temperature sensor has been [modified to have a nRF51822-QFAC](https://twitter.com/NZSmartie/status/1123519600325316608) for the extra RAM (32K) and Flash (256K)
  - Builds or this board only need to apply the `boards/xiaomi_bt_sensor_dev.overlay` file during build.
- The end goal is to build firmware that will work on unmodified board.

## Requirements

- Zephyr's West tool
  ```bash
  python3 -m pip install west
  ```
- Zephyr SDK
  > *TODO:* Add instructions for downloading Zephyr SDK to match current zephyr version.

## Building

```bash
# Use west to clone our repository and set up appropriate project structure
west init -m https://github.com/NZSmartie/MeshTemp && cd MeshTemp
# Checkout all required modules as per MeshTemp/west.yml
west update
# Build the firmware
cd MeshTemp && west build
```

### Building for development

The modded board used for development reasons uses an overlay file that targets the nRF51822 QFAC. Building for this target is done by

```bash
west build -- -DUSE_DEV_BOARD=1
```

## Progress

- [X] Display driver for Zephyr is implemented
  - [X] Display is mapped out
  - [X] Display sensor values
- [X] Mapped GPIO
  - Single button on back
  - Battery measure through ADC 4
- [X] Measure battery voltage through ADC
- [X] Read sensor data (Zephyr has built in support for SHT3Xd)
- [X] Bluetooth support
  - [X] Implement Bluetooth Environmental Sensing Service (ESS)
  - [X] Require bonded device before allowing read/write to ESS characteristics
  - [ ] Bluetooth Mesh Support
- [ ] Power Management (power saving)
