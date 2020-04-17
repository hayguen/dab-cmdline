#!/bin/bash

# requires
# sudo apt-get install clang-format

# execute without arguments for testing
# execute appending " | bash" to reformat all the files

find . -regex '.*\.\(cpp\|hpp\|c\|cc\|cxx\)' -printf "clang-format -i -style=Google \"%p\"\n"
