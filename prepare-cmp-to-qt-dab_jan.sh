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
fi

find ${GIT_TARGET} -regex '.*\.\(cpp\|hpp\|h\|c\|cc\|cxx\)' -printf "clang-format -i -style=Google \"%p\"\n" |bash

