#!/bin/bash

# simple Xephyr wrapper for testing a WM in a nested X session

case "$1" in 
    s|start) 
        [ `pgrep Xephyr` ] && exit 1
        Xephyr -ac -screen 1200x700 :1 &
        sleep 0.5
        DISPLAY=:1
        urxvt -display :1 #-e sh -c "~/.xinitrc a"
#        urxvt -display :1 &
        ;;
    *) 
        [ `pgrep Xephyr` ] && pkill Xephyr || echo "Xephyr not running"
        DISPLAY=:0
        exit 0
        ;;
esac
