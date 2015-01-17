#!/bin/bash

case "$1" in 
    s) 
        [ `pgrep Xephyr` ] && exit 1
        Xephyr -ac -screen 800x500 :1 &
        sleep 0.5
        DISPLAY=:1
        urxvt -display :1 -e sh -c "~/.xinitrc aa" & disown
        ;;
    *) 
        [ `pgrep Xephyr` ] && pkill Xephyr || echo "Xephyr not running"
        DISPLAY=:0
        exit 0
        ;;
esac
