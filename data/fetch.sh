#!/bin/sh
# Download the open datasets used to build dict.tsv (all MIT licensed).
set -e
cd "$(dirname "$0")"
mkdir -p raw
curl -sL -o raw/jieba_dict.txt https://raw.githubusercontent.com/fxsjy/jieba/master/jieba/dict.txt
curl -sL -o raw/phrase_pinyin.txt https://raw.githubusercontent.com/mozillazg/phrase-pinyin-data/master/large_pinyin.txt
curl -sL -o raw/char_pinyin.txt https://raw.githubusercontent.com/mozillazg/pinyin-data/master/pinyin.txt
wc -l raw/*.txt
