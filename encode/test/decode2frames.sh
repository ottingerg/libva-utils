#!/bin/sh
gst-launch-1.0 --gst-debug-level=3 filesrc location=$1 ! decodebin ! queue ! autovideoconvert ! pngenc ! multifilesink location="frame%d.png"
