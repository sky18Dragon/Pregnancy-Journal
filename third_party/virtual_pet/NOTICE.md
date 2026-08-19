# Virtual Pet Upstream Notice

This directory records the upstream material used by the Sticky desktop-pet
core. The firmware contains small adapted portions rather than complete
upstream applications.

| Project | Commit | License | Material used |
| --- | --- | --- | --- |
| [TamaPoke](https://github.com/socquique/TamaPoke) | `37ba1c4805680652632464aed845c4146e71a4a0` | MIT code | Needs, sleep, offline progression, care streak, bond, user-present evolution |
| [esp32-artoria-tamagotchi](https://github.com/fluphus/esp32-artoria-tamagotchi) | `448dbe158595c1db9f1d2fc37caa15810c53de1c` | MIT | Versioned save records, checksums, rotating slots, non-blocking animation queue |
| [openclaw-tamagotchi](https://github.com/katolikov/openclaw-tamagotchi) | `4b3afacc911ff9c8c0bc74c779fb7c926cd004f9` | MIT | State-keyed dialogue structure and phrase source archive |
| [ESP32-TamaPetchi](https://github.com/CyberXcyborg/ESP32-TamaPetchi) | `ade0db1111811ef7cbc343149368a7d781aa4e31` | MIT | Seven-level mood vocabulary, personality evidence, action memory concepts |

The TamaPoke Pokémon names, Pokémon sprites, PMD SpriteCollab artwork, and case
files are outside the MIT code license and are not part of the Sticky asset
library. Sticky uses its original rabbit artwork.

The source audit also examined `siegerts/tama96` at commit
`b4fd018e49a2361c11bbbd39f428ee2402b98bef`. Its repository had no explicit
license at the audited commit, so no source or content from it is included.

The complete MIT notices are stored beside this file.
