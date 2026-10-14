# pinyin-data project notice

This directory contains source data from
[`mozillazg/pinyin-data`](https://github.com/mozillazg/pinyin-data), pinned to
commit `923b108dc5d45dee061324c011b478fb649f8b73` on 2026-09-25.

Imported files:

| File | Upstream SHA-256 | Use in this project |
| --- | --- | --- |
| `kMandarin_8105.txt` | `1b1546f6e190c0f1cbf23e3bb436b41c3ace16f852be4daebc2c7295636628a4` | Primary reading for GB2312 Level-1 characters |
| `kHanyuPinlu.txt` | `260ef08aef066ae5e798ed9933e305094d03a87a5d0bcbced6d740cb9084f2f8` | Stable tie-breaker after explicit tiers and frequency bucket |
| `gb2312_fallback.txt` | Project excerpt; see file header | Readings for three GB2312 Level-1 characters absent from the 8105 table |
| `LICENSE` | `9c048697be2502a16e8bcb282d5d465a07295b2def0ffb05a269c5d39dbe1586` | Upstream MIT license |

`gb2312_fallback.txt` is an excerpt of upstream `pinyin.txt` at the same pinned
commit. The upstream file SHA-256 is
`621f8ca9eff8519f47e2b17b564fd318161e13bca07eea8c8e04993cd5d3b52e`.

The generated firmware index is a deterministic, tone-free, single-character
candidate table. This directory supplies readings and a non-numeric common-set
tie-breaker. Numeric frequency buckets and project-owned candidate tiers are
documented separately under `third_party/unicode-unihan/` and
`assets/chinese-input/candidate-priority.tsv`.
