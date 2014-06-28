#!/bin/sh

set -e

ROOTUID="0"
if [ "$(id -u)" -ne "$ROOTUID" ] ; then
    echo "This script must be executed with root/sudo privileges!"
    exit 1
fi


while true; do
    read -p "Do you wish to uninstall GZ3Doom? [y/N] " yn
    case $yn in
        [Yy]* ) rm -rf /opt/GZ3Doom
                rm -f  /usr/local/bin/gz3doom
                rm -f  /usr/local/share/applications/gz3doom.desktop
                rm -f  /usr/local/share/pixmaps/gz3doom.png

                # remove these directories only if they're empty
                rmdir /usr/local/bin
                rmdir /usr/local/share/applications
                rmdir /usr/local/share/pixmaps
                rmdir /usr/local/share

                echo "All files were successfully removed."
                exit;;

        [Nn]* ) exit;;
        * ) break;;
    esac
done
