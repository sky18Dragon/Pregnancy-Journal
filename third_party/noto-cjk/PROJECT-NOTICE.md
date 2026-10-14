# Noto Sans CJK project notice

The generated Chinese input font asset uses `NotoSansSC-Medium.otf` from
[`notofonts/noto-cjk`](https://github.com/notofonts/noto-cjk), pinned to commit
`f8d157532fbfaeda587e826d4cd5b21a49186f7c`.

| Item | Value |
| --- | --- |
| Source path | `Sans/SubsetOTF/SC/NotoSansSC-Medium.otf` |
| Source SHA-256 | `7633f5a016d4dd95e685a69633d818aabc4644c4b08e26bd35b1b30c45ed5dda` |
| Source size | 8,345,436 bytes |
| Git blob | `fafdfebf3103db4dc81d33ebd55b6786e08c9ebe` |
| License | SIL Open Font License 1.1 |
| Local license SHA-256 | `6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2` |

The original font binary is not committed. Run
`python3 tools/fetch_chinese_input_font.py` to download it into the ignored
`tools/.cache/` directory and verify its SHA-256 before regeneration. The
committed `third_party/noto-cjk/LICENSE` applies to the generated embedded font
bitmaps. The Reserved Font Name requirement remains in force; modified font
software must not be redistributed under a reserved name.
