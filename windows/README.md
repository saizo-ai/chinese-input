# ZhiPin for Windows (TSF text service)

A Text Services Framework input processor for Windows 10/11 x64. The shared
engine in `../core` is compiled directly into `ZhiPin.dll`; `dict.tsv` sits
next to the DLL; learned phrases live in `%APPDATA%\ZhiPin\user_dict.tsv`.

## Building (CI)

GitHub Actions (`windows-latest`, MSVC 2022 + Inno Setup 6) runs:

```bat
cd windows
cmake -B build -A x64
cmake --build build --config Release
iscc installer.iss
```

The installer lands in `windows/dist/ZhiPin-Setup-<version>.exe`.
`installer.iss` expects `..\data\dict.tsv` to exist (run
`python3 ../data/build_dict.py` after `../data/fetch.sh` first).

## Building locally on Windows

Install Visual Studio 2022 (Desktop C++ workload), CMake and
[Inno Setup 6](https://jrsoftware.org/isinfo.php), then run the same three
commands from a "x64 Native Tools" prompt. For a quick dev loop you can skip
the installer and register the DLL by hand from an elevated prompt:

```bat
copy ..\data\dict.tsv build\Release\
regsvr32 build\Release\ZhiPin.dll
:: undo: regsvr32 /u build\Release\ZhiPin.dll
```

Registration writes HKLM and TSF profiles, so it needs elevation. After
registering, add the keyboard in Settings (see below). Text services load
into every process that accepts text input; to update the DLL you must
unregister and sign out (or at least restart the apps using it).

## Enabling the IME

1. Settings → Time & Language → Language & region.
2. Add **Chinese (Simplified, China)** if it is not present.
3. Chinese (Simplified, China) → Language options → Keyboards → **Add a
   keyboard** → **直拼 ZhiPin**.
4. Switch input methods with Win+Space.

## Usage

| Key | Action |
| --- | --- |
| a–z | compose pinyin (exact match, `'` separates syllables) |
| Shift (tap alone) | toggle Chinese / English |
| 1–9 | pick candidate |
| Space | commit highlighted candidate |
| Enter | commit the raw pinyin as typed |
| Esc | cancel composition |
| `-` `=` / PgUp PgDn | page candidates |
| ↑ ↓ | move highlight |
| Ctrl+Delete | permanently delete a learned (★) phrase |

Committing a multi-selection composition saves the combo locally; typing the
same pinyin later shows it first, marked with ★.
