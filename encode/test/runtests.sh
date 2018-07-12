#!/bin/bash
# runtests w h inputfile quantizerparameter bitrate max_vbr_bitrate fps outfile encoder

if [ $# -ne 9 ]
then
echo "Usage: $0 Width Height INPUTFILE Quantizationparameter Bitrate MaxVBRBitrate FPS OUTFILE ENCODER"
exit -1
fi

ENCODER=$9
OUTFILE=$8
LOGFILE=log.txt

echo "${ENCODER} $1 $2 $3 ${OUTFILE}_cqp$4.ivf --qp $4 -f $7"
${ENCODER} $1 $2 $3 ${OUTFILE}_cqp$4.ivf --qp $4 -f $7
echo -n "FILE: ${OUTFILE} TEST 1 - CQP $4: " >> ${LOGFILE}
if [ $? -ne 0 ]
then
  echo "FAIL" >> ${LOGFILE}
else
  echo "PASS" >> ${LOGFILE}
fi

echo "${ENCODER} $1 $2 $3 ${OUTFILE}_cqp$4_svct3_l012.ivf --qp $4 --temp_svc 3 --intra_period 32 -f $7"
${ENCODER} $1 $2 $3 ${OUTFILE}_cqp$4_svct3_l012.ivf --qp $4 --temp_svc 3 --intra_period 32 -f $7
echo -n "FILE: ${OUTFILE} TEST 2 - CQP $4 SVCT 3: " >> ${LOGFILE}
if [ $? -ne 0 ]
then
  echo "FAIL" >> ${LOGFILE}
else
  echo "PASS" >> ${LOGFILE}
fi


echo "${ENCODER} $1 $2 $3 ${OUTFILE}_cqp$4_svct2_l01.ivf --qp $4 --temp_svc 2 -f $7"
${ENCODER} $1 $2 $3 ${OUTFILE}_cqp$4_svct2_l01.ivf --qp $4 --temp_svc 2 -f $7
echo -n "FILE: ${OUTFILE} TEST 3 - CQP $4 SVCT 2: " >> ${LOGFILE}
if [ $? -ne 0 ]
then
  echo "FAIL" >> ${LOGFILE}
else
  echo "PASS" >> ${LOGFILE}
fi

./vp8halfrate < ${OUTFILE}_cqp$4_svct3_l012.ivf > ${OUTFILE}_cqp$4_svct3_l01.ivf
./vp8halfrate < ${OUTFILE}_cqp$4_svct3_l01.ivf > ${OUTFILE}_cqp$4_svct3_l0.ivf
./vp8halfrate < ${OUTFILE}_cqp$4_svct2_l01.ivf > ${OUTFILE}_cqp$4_svct2_l0.ivf
echo "${ENCODER} $1 $2 $3 ${OUTFILE}_cbr$5.ivf --fb $5 --rcmode 1 -f $7"
${ENCODER} $1 $2 $3 ${OUTFILE}_cbr$5.ivf --fb $5 --rcmode 1 -f $7
echo -n "FILE: ${OUTFILE} TEST 4 - CBR $5: " >> ${LOGFILE}
if [ $? -ne 0 ]
then
  echo "FAIL" >> ${LOGFILE}
else
  echo "PASS" >> ${LOGFILE}
fi
echo "${ENCODER} $1 $2 $3 ${OUTFILE}_cbr$5_svct3_l012.ivf --fb $5 --rcmode 1 --temp_svc 3 --intra_period 32 -f $7"
${ENCODER} $1 $2 $3 ${OUTFILE}_cbr$5_svct3_l012.ivf --fb $5 --rcmode 1 --temp_svc 3 --intra_period 32 -f $7
echo -n "FILE: ${OUTFILE} TEST 5 - CBR $5 SVCT 3: " >> ${LOGFILE}
if [ $? -ne 0 ]
then
  echo "FAIL" >> ${LOGFILE}
else
  echo "PASS" >> ${LOGFILE}
fi
echo "${ENCODER} $1 $2 $3 ${OUTFILE}_cbr$5_svct2_l01.ivf --fb $5 --rcmode 1 --temp_svc 2 -f $7"
${ENCODER} $1 $2 $3 ${OUTFILE}_cbr$5_svct2_l01.ivf --fb $5 --rcmode 1 --temp_svc 2 -f $7
echo -n "FILE: ${OUTFILE} TEST 6 - CBR $5 SVCT 2: " >> ${LOGFILE}
if [ $? -ne 0 ]
then
  echo "FAIL" >> ${LOGFILE}
else
  echo "PASS" >> ${LOGFILE}
fi
./vp8halfrate < ${OUTFILE}_cbr$5_svct3_l012.ivf > ${OUTFILE}_cbr$5_svct3_l01.ivf
./vp8halfrate < ${OUTFILE}_cbr$5_svct3_l01.ivf > ${OUTFILE}_cbr$5_svct3_l0.ivf
./vp8halfrate < ${OUTFILE}_cbr$5_svct2_l01.ivf > ${OUTFILE}_cbr$5_svct2_l0.ivf
echo "${ENCODER} $1 $2 $3 ${OUTFILE}_vbr$5_$6.ivf --fb $5 --vbr_max $6 --rcmode 2 -f $7"
${ENCODER} $1 $2 $3 ${OUTFILE}_vbr$5_$6.ivf --fb $5 --vbr_max $6 --rcmode 2 -f $7
echo -n "FILE: ${OUTFILE} TEST 7 - VBR $5_$6: " >> ${LOGFILE}
if [ $? -ne 0 ]
then
  echo "FAIL" >> ${LOGFILE}
else
  echo "PASS" >> ${LOGFILE}
fi
echo "${ENCODER} $1 $2 $3 ${OUTFILE}_vbr$5_$6_svct3_l012.ivf --fb $5 --vbr_max $6 --rcmode 2 --temp_svc 3 --intra_period 32 -f $7"
${ENCODER} $1 $2 $3 ${OUTFILE}_vbr$5_$6_svct3_l012.ivf --fb $5 --vbr_max $6 --rcmode 2 --temp_svc 3 --intra_period 32 -f $7
echo -n "FILE: ${OUTFILE} TEST 8 - VBR $5_$6 SVCT 3: " >> ${LOGFILE}
if [ $? -ne 0 ]
then
  echo "FAIL" >> ${LOGFILE}
else
  echo "PASS" >> ${LOGFILE}
fi
echo "${ENCODER} $1 $2 $3 ${OUTFILE}_vbr$5_$6_svct2_l01.ivf --fb $5 --vbr_max $6 --rcmode 2 --temp_svc 2 -f $7"
${ENCODER} $1 $2 $3 ${OUTFILE}_vbr$5_$6_svct2_l01.ivf --fb $5 --vbr_max $6 --rcmode 2 --temp_svc 2 -f $7
echo -n "FILE: ${OUTFILE} TEST 9 - VBR $5_$6 SVCT 2: " >> ${LOGFILE}
if [ $? -ne 0 ]
then
  echo "FAIL" >> ${LOGFILE}
else
  echo "PASS" >> ${LOGFILE}
fi
./vp8halfrate < ${OUTFILE}_vbr$5_$6_svct3_l012.ivf > ${OUTFILE}_vbr$5_$6_svct3_l01.ivf
./vp8halfrate < ${OUTFILE}_vbr$5_$6_svct3_l01.ivf > ${OUTFILE}_vbr$5_$6_svct3_l0.ivf
./vp8halfrate < ${OUTFILE}_vbr$5_$6_svct2_l01.ivf > ${OUTFILE}_vbr$5_$6_svct2_l0.ivf
