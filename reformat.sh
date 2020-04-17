#!/bin/bash

# requires
# sudo apt-get install clang-format

# execute without arguments for testing
# execute appending " | bash" to reformat all the files

GIT_TOP_DIR=$(git rev-parse --show-toplevel)
if [ ! "$(pwd)" = "$GIT_TOP_DIR" ]; then
  echo "execute this from $GIT_TOP_DIR"
  exit 1
fi

find . -regex '.*\.\(cpp\|hpp\|h\|c\|cc\|cxx\)' -printf "clang-format -i -style=Google \"%p\"\n"
