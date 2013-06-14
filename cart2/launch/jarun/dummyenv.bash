#!/bin/bash
export ROBOT=sim
export LOCATION=jarun
export JOYSTICK=/dev/input/js0
export NAMESPACE=dummy
export TF_PREFIX=dummy
export USE_TF_PREFIX=1
export USE_UWSIM=0
export USE_RVIZ=0
export USE_CNR_RADIO=0
export IS_SIM=1
export IS_REMOTE=0
export USE_BENCH_RADIO=0
export NO_NOISE=1
export LOGDIR=`pwd`/logs/
mkdir -p ${LOGDIR}
export ENABLE_LOGGING=0
export USE_VR=0
export USE_CART=0
export IS_BART=1
export USE_LOCAL_FIX=1
export SIM_MODEL=`rospack find cart2`/data/config/cart2_model.xml
export YAML_MODEL=`rospack find cart2`/data/config/model.yaml