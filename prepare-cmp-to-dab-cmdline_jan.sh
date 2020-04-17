#!/bin/bash

GIT_TOP_DIR=$(git rev-parse --show-toplevel)
if [ ! "$(pwd)" = "$GIT_TOP_DIR" ]; then
  echo "execute this from $GIT_TOP_DIR"
  exit 1
fi

GIT_TARGET="../dab-cmdline_jan"
GIT_URL="https://github.com/JvanKatwijk/dab-cmdline.git"

if /bin/true; then
  if [ -d "${GIT_TARGET}" ]; then
    echo "directory ${GIT_TARGET} already exists. remove that manually first"
    exit 1
  fi
  git clone ${GIT_URL} ${GIT_TARGET}

  echo "removing and moving files and directories .."

  rm -rf ${GIT_TARGET}/dab-scanner/
  rm -rf ${GIT_TARGET}/example-1/
  rm -rf ${GIT_TARGET}/example-2/
  rm -rf ${GIT_TARGET}/example-3/
  rm -rf ${GIT_TARGET}/example-4/
  rm -rf ${GIT_TARGET}/example-5/
  rm -rf ${GIT_TARGET}/example-6/
  rm -rf ${GIT_TARGET}/example-7/
  rm -rf ${GIT_TARGET}/example-8/
  rm -rf ${GIT_TARGET}/example-9/
  rm -rf ${GIT_TARGET}/python-example/

  mv ${GIT_TARGET}/library/includes ${GIT_TARGET}/
  mkdir ${GIT_TARGET}/includes/protection
  mv ${GIT_TARGET}/includes/support/protection.h     ${GIT_TARGET}/includes/protection/
  mv ${GIT_TARGET}/includes/support/protTables.h     ${GIT_TARGET}/includes/protection/
  mv ${GIT_TARGET}/includes/support/eep-protection.h ${GIT_TARGET}/includes/protection/
  mv ${GIT_TARGET}/includes/support/uep-protection.h ${GIT_TARGET}/includes/protection/

  mv ${GIT_TARGET}/library/src ${GIT_TARGET}/
  mkdir ${GIT_TARGET}/src/protection
  mv ${GIT_TARGET}/src/support/protection.cpp     ${GIT_TARGET}/src/protection/
  mv ${GIT_TARGET}/src/support/protTables.cpp     ${GIT_TARGET}/src/protection/
  mv ${GIT_TARGET}/src/support/eep-protection.cpp ${GIT_TARGET}/src/protection/
  mv ${GIT_TARGET}/src/support/uep-protection.cpp ${GIT_TARGET}/src/protection/

  mv ${GIT_TARGET}/device-handler.h     ${GIT_TARGET}/devices/

  mv ${GIT_TARGET}/library/dab-api.cpp  ${GIT_TARGET}/src/
  rm -rf ${GIT_TARGET}/library/cmake/
  rm     ${GIT_TARGET}/library/CMakeLists.txt

fi

echo "reformatting sources via clang-format .."
find ${GIT_TARGET} -regex '.*\.\(cpp\|hpp\|h\|c\|cc\|cxx\)' -printf "clang-format -i -style=Google \"%p\"\n" |bash

