# 直拼 ZhiPin

A pinyin input method for macOS and Windows that respects exactly what you type.

- **Exact matching only** — no fuzzy pinyin (`zh` ≠ `z`, `ing` ≠ `in`), no
  auto-correction, no guessing. If the pinyin doesn't parse, you get your raw
  letters back.
- **Word & sentence conversion** — up to 10 characters per composition,
  converted over a ~465k-entry dictionary with corpus frequencies.
- **Emoji by name** — ~1,800 emoji are typed via the pinyin of their Chinese
  names (from Unicode CLDR): `weixiao` → 😊, `huojian` → 🚀, `xigua` → 🍉.
  They rank just below the hanzi word of the same pinyin, at most 3 per key.
  (Windows candidate window may draw some emoji monochrome — GDI limitation —
  but the committed text is always the real emoji.)
- **Probability-ranked candidates** — every candidate is scored by
  log P(candidate) + log P(best reading of the rest of your input) over a
  unigram word lattice, recomputed at each step of a chained composition. A
  longer word outranks a shorter one only when the probabilities say so.
- **Single-character completion** — while typing a chain, only the syllable
  currently being typed is completed (`zhongg` → 中国). Never predicts extra
  characters beyond what you typed.
- **Local phrase learning** — every combo you commit is saved *on your
  machine* (never uploaded) and marked ★ in the candidate list. Placement is
  adaptive: the first time you use a combo it ranks just below the best
  natural candidate; from the second use on it takes the top spot. Learned
  phrases matching only part of your input rank by probability alongside
  dictionary words. Delete one permanently with the ✕ button or
  <kbd>⌃⌫</kbd> (macOS) / <kbd>Ctrl+Del</kbd> (Windows).
- **Shift toggles 中/英** — tap Shift alone to switch between Chinese and
  English passthrough on both platforms.

Downloads: see the [GitHub Pages site](https://saizo-ai.github.io/chinese-input-site/)
or grab installers from [Releases](https://github.com/saizo-ai/chinese-input/releases).

## How is this different from Sogou (搜狗输入法)?

Sogou optimizes for guessing what you probably meant; ZhiPin refuses to guess
by design. Concretely:

| | Sogou 搜狗 | 直拼 ZhiPin |
| --- | --- | --- |
| Fuzzy pinyin | Configurable (z≈zh, l≈n, in≈ing…) | Never — wrong pinyin gives no candidates |
| Typo correction | Yes ("hoa" → 好) | No — raw letters stay raw |
| Prediction | Next-word / whole-phrase / cloud candidates | Only completes the syllable being typed |
| Language model | Large personalized model + cloud assistance | Local unigram lattice with best-suffix lookahead |
| Trending words | Cloud lexicons updated constantly | Fixed ~465k dictionary until you rebuild it |
| Your typing data | Sent to cloud for candidates/sync/telemetry | Never leaves your machine; no network code |
| Learned phrases | Opaque personal model, account sync | One readable TSV; ★-marked, ✕ to delete an entry |
| Learning behavior | Continuous reordering based on habits | 1st use → 2nd place, 2nd use → 1st place, partial matches rank by probability |
| Extras | Skins, voice, translation, ads | Emoji by pinyin name; nothing else |
| Source | Proprietary (Tencent) | MIT, small enough to read in an afternoon |

Sogou is genuinely better at long-sentence conversion accuracy and fresh
vocabulary. ZhiPin is better if you value determinism and ownership: the same
keystrokes always produce the same candidates, nothing is transmitted
anywhere, and every remembered phrase is a line of text you can see and
delete.

## Key bindings

| Key | Action |
| --- | --- |
| `a–z` | type pinyin |
| `'` | explicit syllable break (`xi'an` → 西安) |
| `1–9` | pick candidate |
| `Space` | pick highlighted candidate (default: first) |
| `Enter` | commit raw letters as typed |
| `Esc` | cancel composition |
| `-` / `=`, `PgUp` / `PgDn` | page candidates |
| `↑` / `↓` | move highlight |
| `Backspace` | delete a letter / undo last pick |
| `⌃⌫` / `Ctrl+Del` | permanently delete highlighted ★ learned phrase |
| `Shift` (tap alone) | toggle Chinese / English |

Full-width punctuation is emitted in Chinese mode (`,` → `，`, `.` → `。`,
`\` → `、`, paired `“”`/`‘’`, …).

## Repository layout

```
core/     shared engine: C++17, no dependencies (segmentation, dictionary
          lattice + sentence DP, user phrase store). `make -C core test`
data/     dictionary build: fetch.sh downloads MIT-licensed sources
          (jieba, pinyin-data, phrase-pinyin-data), build_dict.py emits dict.tsv
macos/    InputMethodKit app in Swift. `make -C macos install`
windows/  TSF text service DLL in C++ + Inno Setup installer (built by CI)
site/     static download page for GitHub Pages
```

## Building

### macOS (local)

```sh
./data/fetch.sh && python3 data/build_dict.py   # build dict.tsv once
make -C core test                                # engine tests
make -C macos install                            # build + install to ~/Library/Input Methods
```

Then: System Settings → Keyboard → Input Sources → **+** → Simplified Chinese
→ **直拼**. Log out and back in if it doesn't appear.

`make -C macos pkg` produces the distributable installer.

### Windows

Built by the release workflow on GitHub Actions (MSVC + Inno Setup); see
[windows/README.md](windows/README.md) for local build steps.

### Releasing

Tag a version to build and publish both installers:

```sh
git tag v0.1.0 && git push --tags
```

## How learning works

When you commit a composition (via candidate selection), the full pinyin you
typed and the resulting text are appended to a plain TSV on your machine:

- macOS: `~/Library/Application Support/ZhiPin/user_dict.tsv`
- Windows: `%APPDATA%\ZhiPin\user_dict.tsv`

Next time you type the identical pinyin, that entry ranks first and is marked
★. Deleting it from the candidate window (or editing the TSV) removes it
permanently. Uninstalling does not delete your user dictionary.

## License & data attribution

Code is MIT licensed. Dictionary data is generated from:

- [jieba](https://github.com/fxsjy/jieba) — word frequencies (MIT)
- [mozillazg/pinyin-data](https://github.com/mozillazg/pinyin-data) — character readings (MIT)
- [mozillazg/phrase-pinyin-data](https://github.com/mozillazg/phrase-pinyin-data) — phrase readings (MIT)
