#!/bin/bash

TOPDIR=`readlink -f \`dirname $0\``
DEP_DIR=`readlink -f $TOPDIR/deps`
CMAKE_COMMAND="${DEP_DIR}/usr/local/oceanbase/devtools/bin/cmake"
CPP_STANDARD=11
CXX_ABI=0

input_args=()
build() {
  for i in "$@"; do
    if [[ "$i" == "--cpp11" ]]; then
      CPP_STANDARD=11
    elif [[ "$i" == "--cpp20" ]]; then
      CPP_STANDARD=20
    elif [[ "$i" == "--cxxabiv1" ]]; then
      CXX_ABI=1
    else
      input_args+=("$i")
    fi
  done
  export CPP_STANDARD=${CPP_STANDARD}
  export CXX_ABI=${CXX_ABI}
  build_dir="build"
  cd $TOPDIR/deps/ && bash dep_create.sh
  mkdir -p $TOPDIR/$build_dir
  cd $TOPDIR/$build_dir && ${CMAKE_COMMAND} ${TOPDIR} "${input_args}" -DCPP_STANDARD=${CPP_STANDARD} -D_GLIBCXX_USE_CXX11_ABI=${CXX_ABI}
}

case "X$1" in
    Xclean)
        find . -maxdepth 1 -type d -name 'build*' | xargs rm -rf
        ;;
    *)
        build "$@" -DCMAKE_BUILD_TYPE=RelWithDebInfo
        ;;
esac
