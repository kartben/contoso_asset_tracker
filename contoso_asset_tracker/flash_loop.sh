#!/bin/bash
i=0
port=$1

if [ "$#" -ne 1 ]; then
    echo "Syntax: ./flash_loop.sh [usb_device]"
    exit
fi

while true
do
    if test -e $port; then
        arduino-cli upload -p $port -b arduino:samd:mkrgsm1400 -i contoso_asset_tracker.arduino.samd.mkrgsm1400.bin -t
        ((i++))
        echo "Successfuly flash device #" $i
        afplay /System/Library/Sounds/Ping.aiff
    fi
    sleep 0.2
done
