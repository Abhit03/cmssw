#!/bin/sh

function die { echo $1: status $2 ;  exit $2; }

pushd ${LOCAL_TMP_DIR}

echo "cmsRun waiting_thread_cfg.py"
cmsRun --parameter-set ${LOCAL_TEST_DIR}/waiting_thread_cfg.py || die 'Failed in waiting_thread_cfg.py' $?

echo "cmsRun acquireTest_cfg.py"
cmsRun --parameter-set ${LOCAL_TEST_DIR}/acquireTest_cfg.py || die 'Failed in acquireTest_cfg.py' $?

popd
