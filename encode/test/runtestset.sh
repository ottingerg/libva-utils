#!/bin/bash
VIDEODIR=~/Videos/
ENCODER=../vp8enc

if [ ! -z "$1" ]
  then
    ENCODER=$1
fi

PLAYLIST=${ENCODER}_test_playlist.txt

start=$(date +%s)
./runtests.sh 352 288 ${VIDEODIR}akiyo_w352_h288_f300_fps30.yuv 26 350 500 30 akiyo_w352_h288_f300 ${ENCODER}
./runtests.sh 720 486 ${VIDEODIR}football_w720_h486_f360_fps30.yuv 26 700 1000 30 football_w720_h486_f360 ${ENCODER}
./runtests.sh 1280 720 ${VIDEODIR}stockholm_w1280_h720_f604_fps60.yuv 26 1400 2000 60 stockholm_w1280_h720_f604 ${ENCODER}
./runtests.sh 1920 1080 ${VIDEODIR}tractor_w1920_h1080_f690_fps25.yuv 26 2800 4000 25 tractor_w1920_h1080_f690 ${ENCODER}
./runtests.sh 4096 2160 ${VIDEODIR}crosswalk_w4096_h2160_f300_fps60.yuv 26 5600 8000 60 crosswalk_w4096_h2160_f300 ${ENCODER}
ls -1 *.ivf > ${PLAYLIST}
end=$(date +%s)
runtime=$((end-start))
echo "Testset completed in $runtime seconds."
