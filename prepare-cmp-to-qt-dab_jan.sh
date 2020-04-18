#!/bin/bash

GIT_TOP_DIR=$(git rev-parse --show-toplevel)
if [ ! "$(pwd)" = "$GIT_TOP_DIR" ]; then
  echo "execute this from $GIT_TOP_DIR"
  exit 1
fi

GIT_TARGET="../qt-dab_jan"
GIT_URL="https://github.com/JvanKatwijk/qt-dab.git"

if /bin/true; then
  if [ -d "${GIT_TARGET}" ]; then
    echo "directory ${GIT_TARGET} already exists. remove that manually first"
    exit 1
  fi
  git clone ${GIT_URL} ${GIT_TARGET}

  echo "removing and moving files and directories .."

  rm -rf ${GIT_TARGET}/appimage/
  rm -rf ${GIT_TARGET}/correlation-viewer/
  rm -rf ${GIT_TARGET}/dab-maxi/
  rm -rf ${GIT_TARGET}/dab-mini/
  rm -rf ${GIT_TARGET}/docs/
  rm -rf ${GIT_TARGET}/forms/
  rm -rf ${GIT_TARGET}/i18n/
  rm -rf ${GIT_TARGET}/includes/output/
  rm -rf ${GIT_TARGET}/includes/scopes-qwt6/
  rm -rf ${GIT_TARGET}/server-thread/
  rm -rf ${GIT_TARGET}/service-description/
  rm -rf ${GIT_TARGET}/sound-client/
  rm -rf ${GIT_TARGET}/spectrum-viewer/
  rm -rf ${GIT_TARGET}/src/output/
  rm -rf ${GIT_TARGET}/src/scopes-qwt6/
  rm -rf ${GIT_TARGET}/src/tii-viewer/

  mv ${GIT_TARGET}/includes/support/fft-handler.h ${GIT_TARGET}/includes/support/fft_handler.h
  mv ${GIT_TARGET}/src/support/fft-handler.cpp    ${GIT_TARGET}/src/support/fft_handler.cpp

  mv ${GIT_TARGET}/dab-processor.h                ${GIT_TARGET}/includes/dab-processor.h
  mv ${GIT_TARGET}/dab-processor.cpp              ${GIT_TARGET}/src/dab-processor.cpp

fi

echo "reformatting sources via clang-format .."
find ${GIT_TARGET} -regex '.*\.\(cpp\|hpp\|h\|c\|cc\|cxx\)' -printf "clang-format -i -style=Google \"%p\"\n" |bash

