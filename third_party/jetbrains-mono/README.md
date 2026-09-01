# JetBrains Mono

Bundled UI typeface for reach, selectable in Settings → Display → Font.

## Provenance

| | |
|---|---|
| Upstream | https://github.com/JetBrains/JetBrainsMono |
| Version | v2.304 (released 2023-01-26) |
| Downloaded from | https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip |
| Archive SHA-256 | `6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf` |
| Retrieved | 2026-08-15 |

`OFL.txt` and `AUTHORS.txt` are copied verbatim from the root of that archive.

## Contents

Only the upright weights reach actually renders are vendored, taken from
`fonts/ttf/` in the archive:

| File | Weight |
|---|---|
| `fonts/JetBrainsMono-Regular.ttf` | 400 |
| `fonts/JetBrainsMono-Medium.ttf` | 500 |
| `fonts/JetBrainsMono-SemiBold.ttf` | 600 |
| `fonts/JetBrainsMono-Bold.ttf` | 700 |

Italics and the remaining weights (Thin/ExtraLight/Light/ExtraBold) are omitted
because no render command requests them. The `NL` ("no ligatures") variants and
the variable-axis builds are omitted as well.

These files are compiled into `reach.exe` as `RCDATA` resources by
`resources/reach.rc` and loaded through a DirectWrite in-memory font
collection; they are not installed into Windows.

## License

The font is licensed under the **SIL Open Font License, Version 1.1** — see
`OFL.txt`. It is *not* covered by reach's own license, and per OFL clause 5 it
must remain under the OFL.

Obligations this places on reach:

- Ship `OFL.txt` with the font (this directory satisfies that, and the file is
  also carried into release archives).
- Do not sell the font on its own. Bundling it inside reach is expressly
  permitted, including commercially.
- Do not use the JetBrains name to promote reach.

No in-application credit or about-box attribution is required. JetBrains did
not declare a Reserved Font Name, so the family may be modified or subset
without renaming.
