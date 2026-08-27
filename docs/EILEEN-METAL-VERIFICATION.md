# THE EILEEN — the four-material verification

*2026-08-26, the metal lane + master's salvage. THE EILEEN exists in four
materials: pine (prose), steel (quilt sheet), bread (MIDI), metal (.qm on
ESP32-S3). The statue criterion: a statement is true iff all its renderings
agree. The row-by-row check:*

| Joint (prose manifest) | Steel dependency | Metal .qm | Agrees |
|---|---|---|---|
| Keel: the blink, everything hung from it | `keel` → keelson | `keel=1`, blink ♩=60 (500ms), all cells bound to it | ✓ |
| Stem: water one character at a time | `stem` sensor | nmea.c byte-feed; sentence ⇒ day advances (stem += 1) | ✓ |
| Keelson sealed to stem | `keelson = stem + keel` | same formula, integer, frozen | ✓ |
| Breast-hook grown from keelson | `breast_hook = keelson * 2` | same | ✓ |
| Rigging = the five verbs | `rigging = breast_hook + keelson` | chain via qm_bind/link/effect/view/tick | ✓ |
| Bulwarks read rigging | `bulwarks = rigging - 2` | same | ✓ |
| Ensign above bulwarks | `ensign = bulwarks + 1` | same | ✓ |
| Ensign descends to scuppers | `scuppers = ensign - 1` | same | ✓ |
| Scuppers up to sheerboard | `sheerboard = scuppers + breast_hook` | same | ✓ |
| Figurehead closes on the keel | listener watches sheerboard; log: "the days grew from the keel" | fires when sheered; the host replay ends on exactly that line | ✓ |

**Host replay evidence** (`make eileen-run`, master's own run): 13 lines —
10 days advanced (GGA/RMC/HDT/VHW/DBT/ZDA), each `agree:true`, the chain
evaluating in joint order every day; 1 unknown sentence ignored; bad
checksum and bad format rejected and counted — overboard, never aboard.

**On-target evidence**: 5 pio envs green; merged S3 image
`dist/the-eileen-s3-merged-0x0.bin` (sha 41c77075…). At boot the board
prints the mint receipt (eileen.qm sha256 8721b4bd…), blinks green at ♩=60 —
the same tempo movement I of `the-eileen.mid` — and prints one JSON line
per day over serial. Feed it NMEA sentences (any USB-serial, 115200) and
the days grow.

*One boat, four materials, every rendering agreeing. The statue stands.*
