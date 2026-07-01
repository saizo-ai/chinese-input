#!/usr/bin/env python3
"""Build dict.tsv for the ZhiPin engine.

Sources (downloaded into data/raw/ by fetch.sh):
  - char_pinyin.txt   mozillazg/pinyin-data (MIT): per-character pinyin, all readings
  - phrase_pinyin.txt mozillazg/phrase-pinyin-data (MIT): per-phrase pinyin
  - jieba_dict.txt    fxsjy/jieba (MIT): word/char corpus frequencies

Output: dict.tsv, lines of "key\ttext\tweight" sorted by key, where key is
tone-stripped pinyin syllables joined by apostrophes (u-umlaut written as v).
"""

import re
import sys
import unicodedata
from pathlib import Path

RAW = Path(__file__).parent / "raw"
OUT = Path(__file__).parent / "dict.tsv"

MAX_SYLLABLES = 10
DEFAULT_CHAR_WEIGHT = 10
DEFAULT_PHRASE_WEIGHT = 2
SECONDARY_READING_DIVISOR = 4

# Interjection "syllables" that would poison segmentation ("n" would make the
# input "n" parse as complete and kill single-letter completion).
BANNED_SYLLABLES = {"n", "m", "ng", "hm", "hng"}

SYL_RE = re.compile(r"^[a-z]{1,6}$")


def strip_tones(syl: str) -> str:
    """ni3-hao style isn't used by these datasets; they use combining marks."""
    out = []
    for ch in unicodedata.normalize("NFD", syl.lower()):
        if unicodedata.category(ch) == "Mn":
            if ch == "̈":  # diaeresis: u-umlaut -> v
                if out and out[-1] == "u":
                    out[-1] = "v"
            continue
        out.append(ch)
    return "".join(out)


def is_cjk(ch: str) -> bool:
    cp = ord(ch)
    return (
        0x3400 <= cp <= 0x4DBF
        or 0x4E00 <= cp <= 0x9FFF
        or 0xF900 <= cp <= 0xFAFF
        or 0x20000 <= cp <= 0x3134F
        or cp == 0x3007  # 〇
    )


def syllable_aliases(syls: list[str]) -> list[list[str]]:
    """lve/nve may also be typed lue/nue; emit both spellings."""
    keys = [syls]
    if any(s in ("lve", "nve") for s in syls):
        keys.append([s.replace("ve", "ue") if s in ("lve", "nve") else s for s in syls])
    return keys


def load_jieba_freq() -> dict[str, int]:
    freq: dict[str, int] = {}
    with open(RAW / "jieba_dict.txt", encoding="utf-8") as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 2 and parts[1].isdigit():
                freq[parts[0]] = max(freq.get(parts[0], 0), int(parts[1]))
    return freq


def main() -> None:
    freq = load_jieba_freq()
    entries: dict[tuple[str, str], int] = {}  # (key, text) -> weight
    valid_syllables: set[str] = set()

    # --- single characters ---
    n_chars = 0
    with open(RAW / "char_pinyin.txt", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            m = re.match(r"^U\+([0-9A-F]+):\s*([^#]+)", line)
            if not m:
                continue
            ch = chr(int(m.group(1), 16))
            if not is_cjk(ch):
                continue
            readings = [strip_tones(p.strip()) for p in m.group(2).split(",") if p.strip()]
            readings = [r for r in readings if SYL_RE.match(r) and r not in BANNED_SYLLABLES]
            if not readings:
                continue
            base = freq.get(ch, DEFAULT_CHAR_WEIGHT)
            seen: set[str] = set()
            for i, r in enumerate(readings):
                if r in seen:
                    continue
                seen.add(r)
                w = base if i == 0 else max(base // SECONDARY_READING_DIVISOR, 2)
                for key_syls in syllable_aliases([r]):
                    k = (key_syls[0], ch)
                    entries[k] = max(entries.get(k, 0), w)
                valid_syllables.add(r)
                if r in ("lve", "nve"):
                    valid_syllables.add(r.replace("ve", "ue"))
            n_chars += 1

    # --- phrases ---
    n_phrases = n_dropped = 0
    with open(RAW / "phrase_pinyin.txt", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or ":" not in line:
                continue
            word, _, pinyin = line.partition(":")
            word = word.strip()
            if not (2 <= len(word) <= MAX_SYLLABLES) or not all(is_cjk(c) for c in word):
                n_dropped += 1
                continue
            syls = [strip_tones(s) for s in pinyin.split()]
            if len(syls) != len(word) or not all(
                SYL_RE.match(s) and s in valid_syllables for s in syls
            ):
                n_dropped += 1
                continue
            w = freq.get(word, DEFAULT_PHRASE_WEIGHT)
            for key_syls in syllable_aliases(syls):
                k = ("'".join(key_syls), word)
                entries[k] = max(entries.get(k, 0), w)
            n_phrases += 1

    with open(OUT, "w", encoding="utf-8") as f:
        for (key, text), w in sorted(entries.items()):
            f.write(f"{key}\t{text}\t{w}\n")

    print(
        f"chars={n_chars} phrases={n_phrases} dropped={n_dropped} "
        f"entries={len(entries)} syllables={len(valid_syllables)} -> {OUT}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
