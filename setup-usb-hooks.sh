#!/bin/bash
SCRIPT=$(realpath "$0")
SCRIPT_PATH=$(dirname "$SCRIPT")
echo "ACTION==\"add\", SUBSYSTEM==\"usb\", ATTRS{idVendor}==\"10c4\", RUN+=\"/bin/bash -c 'USER=$USER $SCRIPT_PATH/start-log.sh'\"" | sudo tee /etc/udev/rules.d/hyper.rules
echo "ACTION==\"remove\", SUBSYSTEM==\"usb\", ATTRS{idVendor}==\"10c4\", RUN+=\"$SCRIPT_PATH/stop-log.sh\"" | sudo tee -a /etc/udev/rules.d/hyper.rules
sudo udevadm control --reload
