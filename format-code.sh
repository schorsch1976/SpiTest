#!/bin/sh
find src/ -iname "*.cpp" -exec clang-format -i {} \;
find src/ -iname "*.h" -exec clang-format -i {} \;

find src/ -iname "*.cpp" -exec dos2unix {} \;
find src/ -iname "*.h" -exec dos2unix {} \;
find src/ -iname "*.txt" -exec dos2unix {} \;
