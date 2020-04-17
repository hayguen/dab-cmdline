#!/bin/bash

GIT_TOP_DIR=$(git rev-parse --show-toplevel)
if [ ! "$(pwd)" = "$GIT_TOP_DIR" ]; then
  echo "execute this from $GIT_TOP_DIR"
  exit 1
fi

P=$(basename "$GIT_TOP_DIR")

for GIT_TARGET in $(echo "../$P ../dab-cmdline_jan ../qt-dab_jan") ; do
  B=$(basename "${GIT_TARGET}")
  echo -e "${GIT_TARGET}\t\t$B"
  pushd "${GIT_TARGET}" &>/dev/null
  find . |grep -v '\./\.git/' | sort >../${B}_filelist.txt
  popd &>/dev/null
done

