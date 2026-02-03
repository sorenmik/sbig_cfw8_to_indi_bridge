#!/bin/bash

SKETCH_DIR="filterwheel"

FQBN="Seeeduino:samd:seeed_XIAO_m0"
PORT="/dev/ttyACM0"

PATH="$PATH:/home/spiff/Programs/Arduino IDE/bin"


# # One time install
# arduino-cli config add board_manager.additional_urls \
#     https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
# arduino-cli core update-index
# arduino-cli core install Seeeduino:samd


arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR"
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_DIR"
