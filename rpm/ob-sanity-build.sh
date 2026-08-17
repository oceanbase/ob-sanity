#!/bin/bash

CUR_DIR=$(dirname $(readlink -f "$0"))
TOP_DIR=$CUR_DIR/..
PROJECT_DIR=${1:-"$TOP_DIR"}
PROJECT_NAME=${2:-"ob-sanity"}
VERSION=${3:-"2.0.4"}
RELEASE=${4:-"1"}

# prepare building environment
ID=$(grep -Po '(?<=^ID=).*' /etc/os-release | tr -d '"')
 
if [[ "${ID}"x == "alinux"x ]]; then
  os_version=8
  export RELEASE=$RELEASE.al$os_version
else
  os_version=`grep -Po '(?<=release )\d' /etc/redhat-release`
  export RELEASE=$RELEASE.el$os_version
fi
ob_build_spec=${PROJECT_NAME}.spec
LIB_PATH_BACKUP=$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$LIB_PATH_BACKUP
yum install -y ncurses-devel

TMP_DIR=${TOP_DIR}/${PROJECT_NAME}-tmp.$$
echo "[BUILD] create tmp dirs...TMP_DIR=${TMP_DIR}"
mkdir -p ${TMP_DIR}
mkdir -p ${TMP_DIR}/BUILD
mkdir -p ${TMP_DIR}/RPMS
mkdir -p ${TMP_DIR}/SOURCES
mkdir -p ${TMP_DIR}/SRPMS

CXX_ABI=${CXX_ABI:-0}
export CXX_ABI

echo "[BUILD] make rpms... spec_file=${ob_build_spec}"
rpmbuild -ba --define "_topdir ${TMP_DIR}" --define "version ${VERSION}" --define "release ${RELEASE}" $ob_build_spec || exit 1
echo "[BUILD] make rpms done."

find ${TMP_DIR}/RPMS/ -name "*.rpm" -exec mv '{}' $CUR_DIR \; || exit 2
rm -rf ${TMP_DIR}
