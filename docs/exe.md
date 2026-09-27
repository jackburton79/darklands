# DARKLAND.EXE notes

Working notes on the game's executable: how to find code in it, and the
rules decoded so far. File offsets are hexadecimal, `seg:off` are the
program's own (pre-relocation) addresses. Disassembly with
`ndisasm -b 16 -e <file offset> -o <file offset> DARKLAND.EXE`.

## Structure

- An MZ executable (header 0x1800 bytes, 1463 relocations) whose load
  image is only 65366 bytes: the **root**. The other 1.6 MB are
  **overlays** of the RTLink overlay manager (Pocket Soft: ".RTLink
  CACHE", "Overlay Manager Internal Reload Stack Overflow"), code and
  data stored uncompressed. Compiled with Microsoft C 6 (large model:
  "MS Run-Time Library - Copyright (c) 1990, Microsoft Corp"); a PKWARE
  library is linked in too.
- **Root segments**: file = 0x1800 + seg · 16 + off. **verified** (e.g.
  06A1:2DC2 is a function prologue at file 0xAFD2).
- **Data segments** at the end of the file: file = seg · 16 + 0x15EC20.
  **verified** for 290E (far strings and tables, file 0x187D00) and
  321A = **DGROUP** (file 0x190DC0: "MS Run-Time Library..." at offset
  8, as MSC puts it; the code loads `mov ax, 0x321A; mov ds, ax`).
  2E38 is another one.
- **Overlay code segments** are elsewhere, not linearly. Their file base
  is found from the far calls (`9A off seg`) into them: the base that
  puts every called offset on a function prologue (`C8 xx xx 00` or
  `55 8B EC`). Found so far: 0E76 → 0x578B0, 1367 → 0x5C7C0,
  18E7 → 0x65C30 (the trade code), 12F7 → 0x3E020, 146D → 0x1CE20,
  150B → 0x8D110, 1551 → 0x1DC60, 18BF → 0x43CA0.
- Calls between overlays often go through RTLink's own mechanism, not
  direct far calls: callers of a function can be missing from a search.

## Finding code

Most game strings are in DGROUP or segment 290E. A string at file
offset f in DGROUP is DS:(f − 0x190DC0); the code refers to it with a
`push imm16` or `mov reg, imm16` of that offset, and each string is
usually used once: searching those bytes gives the code that uses it.
E.g. "%5upf  %Fs  %3dq  %3dlbs" (DS:3055) is pushed at 0x682F1, in the
function that draws the merchant's scroll.

## Runtime and globals

- 06A1:2DC2 long multiply, 06A1:2D28 signed long divide (MSC helpers,
  arguments on the stack, the callee pops them); 06A1:2096 sprintf.
- 0410:0008 `random(n)` = (rand() · 2 · n) >> 16, i.e. 0..n−1.
- 1367:0066 `clamp(low, high, value)`.
- 0E76:05EE `attribute(character, index)`: −1 is the leader; index 5 is
  charisma (the order of the character record).
- 0E76:1A7E `location_property(n)` of the current location, a switch
  on n: 0 is the party's reputation there (record +0x12), 1 its word
  (see below).
- DS:00E0.. the date (see "Time and travel"). DS:907B the leader (party slot), DS:907E the current location (index
  into DARKLAND.LOC, −1 in the wilderness; cities are < 92), DS:9785 a
  far pointer to the current city's DARKLAND.CTY record, [DS:7E6A]:0890
  the far pointers to the 200 item definitions (DARKLAND.LST), [DS:7E20]:
  10BE the far pointers to the location records (as DARKLAND.LOC:
  +0x12 reputation, +0x14 flags).

## Trade

All in segment 18E7 (file base 0x65C30); see `TradeView.cpp`.

- **Opening a shop**, 18E7:1C30 (type, name, mask): the quality of the
  shop is the city record's byte at +0x62 + type for types 0..8, 25
  otherwise (10 for type −2, the pawnshop). The types, as named by the
  table at 290E:207F: 0 Blacksmith, 1 Goods Merchant, 2 Swordsmith,
  3 Armorer, 4 Gunsmith, 5 Bowyer, 6 Artificier, 7 Jeweler,
  8 Clothmaker, 9 Physician, 10 Hospital, 11 Nobility. The shops are
  opened through 0E76:21AA; the masks (item flags, see DARKLAND.LST):
  Goods Merchant 0x0002C100, Blacksmith and Arms Outfitter 0x040000FF,
  Swordsmith 0x0000000F or 0x040000FF, Armorer 0x040000C0 or
  0x040000FF, Bowyer 0x083C0030 or 0x040000FF, Clothmaker 0x04000000,
  Artificier 0x0000800E, Physician, herbalist, University, merchant
  0x400, Alchemist 0x800, Foreign Trader 0x003F843F, Pawnshop
  0x2C3EC3FF, stables 0x2000, castle 0x202D, monk 0x0400040C. Which call
  is which place is *inferred*: each guild shop is opened twice (by day
  and at night, presumably) with 0x040000FF, the bowyer with
  0x083C0030; a block of calls with the narrower masks is elsewhere.
- **Stock**, 18E7:3948 (item): the item's flags must share a bit with
  the shop's mask, and flags byte 4 bit 7 must be clear; ammunition
  (types 0x40..0x42) is always there. Otherwise its rarity, +1 if the
  merchant's quality is under 23, −1 over 26, −2 over 29, clamped to
  0..11, and the city size − 1 (1 outside cities) index a table at
  290E:399B (9 × 12 percentages, `kStockChance` in `TradeView.cpp`);
  the item is there if random(100) ≤ that. The city index is read from
  an uninitialized local variable: a bug of the original, which makes
  the size row random. Items are listed in item order.
- **Merchant quality**, 18E7:2B2A (item): (item's default quality +
  shop quality) / 2; 25 for ammunition, pure alchemical components
  (flags 0x00000400) and types 0x17..0x19.
- **Buying price**, 18E7:355E (item), long arithmetic, divisions
  truncated: p = 2 · q · value / 25; in a location, p += p · size · 2 /
  (rarity > 5 ? 100 : −100), size being the city's (2 elsewhere); p +=
  p · (25 − leader's charisma) / 100; in a location, p −= p · t / 100
  with t = clamp(reputation · 50, −999, 99) / 100 (so only a bad
  reputation, down to −9, raises prices); a last term, clamp(value ·
  100, 0, 500) / 1000 with the value of 0E76:1326(4), is always 0; at
  least 1. The location flags: bit 1 or 0x41 add 3 to the rarity, bit 3
  adds random(10) to the quality.
- **Selling price**, 18E7:3756 (item, quality): 0 if flags byte 4 bit 7
  is set; the same from a quarter of the value, the charisma term
  subtracted (p −= p · (25 − charisma) / 100), the reputation term
  clamped at 75 and added; 0 if under 1. Every merchant buys every
  sellable item.
- The prices on the manual's screenshot (p. 29) do not follow these
  rules: it shows a pre-release version.

## Time and travel

- The date: DS:00E0 hour, 00E2 day (1-based), 00E4 month (0-based),
  00E6 year; the month lengths are the words at DS:2776 (31, 28, 31...,
  no leap years). 1367:05C8 `AddHours(n)` (file 0x5CD88) adds hours with
  the carries, after calling the functions that make time pass (0E76:
  255A, and 09C0:1F95 for more than 2 hours); 1367:0680 adds days. They
  are called about 2200 times, mostly by the card flows.
- The hour names are "Matins", "Latins", "Prime", "Terce", "Sexts",
  "Nones", "Vespers", "Compline" (290E:0B01...): "Latins" and "Sexts"
  are the game's spelling, not the manual scan's.
- **Moving on the map** (file 0x5E79E...): the party's screen position is
  DS:2866 (x), DS:2868 (y); a move changes x by 1 pixel every frame and
  y by 1 every other frame (DS:290E toggles).
- **Travel time** (file 0x5EDA0, then 0x5EE68): after every move, the
  tile under the party (its type, from the pixel: row (y + 11) / 4,
  column (x + 16 or 24) / 16) gives a cost in minutes (the switch at
  file 0x60568): 9 by default (plains, farmland, ford, river, bridge,
  castle, city), ocean and major river 40, minor river 30, marsh 50,
  geest 17 / 13, farmland 11, fields and woods 10 / 12, light woods
  14 / 16, forest 18 / 27 / 40 / 60, rocky 20 / 30 / 45 / 67, alps
  98 / 197, road 6. It is added to DS:8672; when that reaches 60, one
  hour passes (`AddHours(1)`, at most one a move) and 60 is subtracted.
  So a diagonal step to a neighbor tile (8 × 4 pixels, 8 moves) takes
  72 minutes on plains, 48 on a road. (The switch also turns multiples
  of 1440 into hours, which its costs never reach.)
- **Water**: entering an ocean (1) or major river (2) tile is refused
  (the step returns 9999) unless the previous tile was water too, or
  0E76:32CE(0x4B) is true (the party has something: item 0x4B is
  "Marsh Vapor"; not checked).

## Reputation

0E76:1B12 turns a reputation into the index of its word, in the list
"a local hero", "respected", "unknown", "suspected", "wanted", "hunted":
over 50 → 0, over 10 → 1, over −10 → 2, over −40 → 3, over −75 → 4,
else 5. **verified** (code).
