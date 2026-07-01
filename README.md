# 直拼 ZhiPin

A pinyin input method for macOS and Windows that respects exactly what you type.

- **Exact matching only** — no fuzzy pinyin (`zh` ≠ `z`, `ing` ≠ `in`), no
  auto-correction, no guessing. If the pinyin doesn't parse, you get your raw
  letters back.
- **Word & sentence conversion** — up to 10 characters per composition,
  converted over a ~465k-entry dictionary with corpus frequencies.
- **Single-character completion** — while typing a chain, only the syllable
  currently being typed is completed (`zhongg` → 中国). Never predicts extra
  characters beyond what you typed.
- **Local phrase learning** — every combo you commit is saved *on your
  machine* (never uploaded) and shown first (marked ★) the next time you type
  the same pinyin. Delete one permanently from the candidate UI with
  <kbd>⌃⌫</kbd> (macOS) / <kbd>Ctrl+Del</kbd> (Windows).
- **Shift toggles 中/英** — tap Shift alone to switch between Chinese and
  English passthrough on both platforms.

Downloads: see the [GitHub Pages site](https://omnific9.github.io/chinese-input-site/)
or grab installers from [Releases](https://github.com/omnific9/chinese-input/releases).

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
