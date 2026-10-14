# Unicode Unihan project notice

The generated `kFrequency-gb2312-level1.txt` file is a filtered extract of
Unicode Unihan 15.1.0 `Unihan_DictionaryLikeData.txt`. The source archive is
pinned to:

- URL: <https://www.unicode.org/Public/15.1.0/ucd/Unihan.zip>
- SHA-256: `a0226610e324bcf784ac380e11f4cbf533ee1e6b3d028b0991bf8c0dc3f85853`
- Extracted property: `kFrequency`
- Filter: GB2312 Level-1 characters

Unicode documents `kFrequency` as a rough frequency measurement with values
from 1 (most frequent) to 5 (least frequent). The property was based on
traditional-Chinese USENET data and is provisional. It is used here only as a
tie-breaker inside explicit project ranking tiers, not as a modern Simplified
Chinese corpus and not as a complete production-IME frequency model.

The project intentionally pins Unicode 15.1.0 because newer Unihan releases no
longer publish `kFrequency`. Run the checked-in extraction tool to reproduce the
filtered file:

```sh
python3 tools/fetch_unihan_frequency.py
```

Unicode data is redistributed under the license in `LICENSE`.
