#!/bin/bash
# 一時保存用フォルダを作成
mkdir -p build
# $1はコマンドライン引数で指定されたファイル名の先頭部分列(ex. hogefuga.cpp なら hog など)
files=$(ls thunder| grep "^$1.*\.cpp$")
code_file=thunder/$(echo $files | head -n 1)
rm -f build/a.out
mkdir -p thunder/combined
code_file_basename=$(basename $code_file)
python3 combiner.py  $code_file thunder/combined/combined_${code_file_basename}
g++ -std=c++20 -I thunder/ -O2 $code_file -o build/a.out