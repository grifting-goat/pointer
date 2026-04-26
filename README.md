## DR SUESS HAND

# Embedded systems project

The goal is to develop a comically large pointer hand that can detect gestures and movements to control a computer over Bluetooth

# Structure

- Bluetooth Hardware Interface Device
- Small Neural Net recognizing simple guestures (1 Dimensional CNN)

# Components

- ESP32-S3 DEVKIT
- 3.7 V Lithium Ion Battery
- TP4056 Charging Module 
- BMI160


# Software

- ESP-IDF
- Python -> PyTorch


# Constraints
- 512 SRAM
    - Bluetooth and I2C stack
- 8 MB PSRAM
    - Inference stack
- 16 MB Flash
    - Binaries + model weights  