#!/bin/bash

if [ $EUID -ne 0 ]
  then
  echo "Run this script as root"
  exit 1
fi

if [ ! -f "build/native/src/core/smarthomed" ]
  then
  echo "Build not found"
  exit 1
fi

echo "Installing binaries"
install -m 755 build/native/src/core/smarthomed /usr/local/bin/
install -m 755 build/native/src/mediator/smarthome-mediator /usr/local/bin/

echo "Creating config directory"
mkdir -p /etc/smarthome
install -m 644 configs/smart_home.yaml /etc/smarthome/
install -m 644 configs/mediator.yaml /etc/smarthome/


echo "Installing systemd service"
install -m 644 systemd/smarthomed.service /etc/systemd/system/

echo "Reloading systemd"
systemctl daemon-reload