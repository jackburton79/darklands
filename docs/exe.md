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
  18E7 → 0x65C30 (the trade code), 1838 → 0xB8840 (the city
  church), 1893 → 0x9F620 (the market), 12F7 → 0x3E020, 146D → 0x1CE20,
  150B → 0x8D110, 1551 → 0x1DC60, 18BF → 0x43CA0.
- Calls between overlays often go through RTLink's own mechanism, not
  direct far calls: callers of a function can be missing from a search.
- **Overlays share segment numbers**: 1838 is the church at file
  0xB8840 and the physician at file 0xA2E20. A segment's base is only
  valid for the overlay it was found in.

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
- 1367:0066 and 0410:001A `clamp(low, high, value)`.
- 1367:07EE the current bell, 1 (Matins, hours 0..2) to 8 (Compline,
  21..23): hour / 3 + 1. 1367:0716 (hour) waits until that hour: it
  adds the hours to it, through midnight if it has passed (1367:086A).
  1367:072A is true from hour 5 to 18.
- 0E76:01A0 `skill(character, index)`, 0E76:02EA adds to a skill;
  0E76:0A72 `add_to_attribute(character, index, amount)`: the result is
  clamped to 1..99 and to the attribute's maximum.
- 0E76:1A8E `location_property(location, n)`: n = 2 is the city size.
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

## The market

$MARKE00.MSG by day, segment 1893 (file base 0x9F620). **verified**
(code); see `CityVisit.cpp`.

- **The card** (1893:0002): options 0..2 (everyday merchants, foreign
  traders, pharmacists) are always there; 3, 4, 5 (Fugger, Medici,
  Hanse) need the location properties 0x0C, 0x0D, 0x0E (DARKLAND.LOC
  record bytes +0x15, +0x16, +0x17); 6 (the Leihhaus) needs property
  0x1F, bit 0x200 of the city record's word at +0x5E (57 cities; not the
  place name: Groningen names a Leihhaus but has no bit). A party wanted
  in the city (0x9C0:20F3) sees card 1 instead of 0. Each option has
  two functions: a chance (the table at DS:EA14, e.g. 1893:089A: 100 −
  [DS:586C] · 6 / 10, [DS:586C] being set for a wanted party; *inferred*:
  the chance of not being recognized) and the action (the switch at
  1893:0266).
- **The merchants**: the actions 1893:045A, 08F2, 0D9C, 13CE open the
  trade screen through 0E76:21AA (type, name, mask): everyday items
  (1, NULL: "Goods Merchant", 0x0002C100), foreign traders (−1,
  "Foreign Trader", 0x003F843F), pharmacists (−1, "herbalist",
  0x00000400), the Leihhaus (−2, "Pawnshop", 0x2C3EC3FF). So the goods
  merchant's quality is the city's shop 1, the foreign traders' and
  herbalist's 25, the pawnshop's 10 (see Trade). No time passes.
- Not reproduced: before trading, a wanted party may be caught
  (random(100) under the chance: state 1), and quests may be offered
  (0E76:3404 (3 or 10, 0, location): cards 2 and 3); after trading, a
  random(100) draws an event (under 2 and under 14: other decks, one
  hour), unless a quest is pending. The cards 7..10 (the merchants'
  descriptions) are not shown by this code.

## Names and random numbers

- 06A1:29A4 `srand`, 06A1:29B6 `rand` (Microsoft C: seed = seed ·
  0x343FD + 0x269EC3, returns bits 16..30); 0410:0008 `random(n)` =
  (rand() · 2 · n) >> 16.
- 1367:0DB4 (seed, kind, slot) names a person: srand(DS:9C4A + seed),
  random(1000) thrown away, then by kind, e.g. kind 0: a man's first name
  (108, the far pointers at 290E:235F), a space, a surname (146, at
  290E:267F); women's names (88) are at 290E:2517. Then srand(time).
  DS:9C4A is a global set at run time (not in the executable's data; not
  found yet), taken as 0 here. `ExeNames.cpp` reads the lists from the
  executable.
- Location property 0x21 of a city is the city record's word +0x56 plus
  DS:9C4A: the seed of its people.

## The physician

$PHYSI00.MSG by day (segment base 0xA2E20, the card at file 0xA2E6A,
the options' switch at file 0xA3128). **verified** (code); see
`CityVisit.cpp`.

- **The physician** is a person of the city (0E76:2C4E, kind 0x37),
  made at the first visit: skill = clamp(1, 99, (P % 10) · (size +
  random(4) − 3)), P the city's property 0x21. His name is 1367:0DB4(P +
  0x320, 0, 0). He speaks with the best healer (0E76:14A4(13) leaves its
  index at DS:991D). Reaching him from the crafts takes an hour (file
  0xA455A). A reputation of −40 or less: card 3, the door shut.
- **Discussing treatments** (file 0xA31D6): an hour; if random(100) is
  over the healer's intelligence + charisma / 2 + the skill (file
  0xA333C), card 9; else a skill of 1 is an idiot (card 10, the party
  leaves), otherwise card 8 with $Text1 "Poor" (under 20), "Modest"
  (40), "Good" (60), "Very Good" (80), "Excellent".
- **Asking his aid** (file 0xA3388), offered when a member's strength is
  under its maximum: an hour; the price is (skill / 10 + 12) pfennigs per
  wounded member ($Number1, $Money1, card 2); the treatment option is
  then on.
- **The treatment** (file 0xA36C0): card 14 if the purse is short; else
  paid, an hour, each wounded member gains clamp(1, 99, skill / 30)
  strength (random(2) − 2 from an idiot), card 13, and a mark (0E76:2930,
  kind 0x36, 20 hours: 0E76:2930 adds them to the date with 1367:09EA)
  disables the treatment until it expires.
- **Alchemical components** (file 0xA35E8): the trade screen (type 9,
  mask 0x400) and an hour if random(100) is at most clamp(0, 75, the
  leader's Speak Common + charisma + (P + month) % 30 + reputation)
  (file 0xA365C), else card 12.
- Not reproduced: the students (training), the night card (1).

## The banks

The Fuggers ($FUGGE00.MSG, file 0xC41E7) and the Medici ($MEDIC00.MSG,
file 0xC6253) follow the same code. **verified** (code); see
`CityVisit.cpp`.

- **Where**: the market offers them, and the Hanseatic League
  ($HANSE00.MSG), when the location properties 0x0C, 0x0D, 0x0E are not
  0: the location record's bytes +0x15, +0x16, +0x17, which are 0x19 for
  every city in DARKLAND.LOC and in the saved games seen.
- **The card**: card 0, or card 2 ("the guards grip their weapons")
  when the local reputation or the party's standing with the bank
  (DS:4BB6 Fuggers, DS:4BB8 Medici; 0 in the executable, raised by one
  at each visit while not over 0) is under 0; with both summing to −100
  or less the party is thrown out (card 5). $Money1 is the letter of
  credit (DS:9072, in florins: one amount, whichever bank issued it).
  Redeeming needs a letter; buying one needs over one florin in the
  purse (DS:906B, the florins alone); the tasks are off on card 2 and
  with a pending quest.
- **Redeeming** (file 0xC4568, 0xC65B8): the letter's florins go to the
  purse, less 6 pfennigs a florin ($Money2), card 6.
- **Buying** (file 0xC4634, 0xC6676): card 3, then a typed line
  (0x9C0:20B7) "Deposit how many Florins?" (DS:5B8F), 10 characters,
  starting with the purse's florins; the number is clamped to 0..500
  and to the purse's florins, which go to the letter. No fee, no time.
- Not reproduced: the standings, the banker's name ($NamedOneName,
  1367:0DB4 with the city record's word +0x56 + 8, + 6 for the Medici),
  the tasks, rewards and politics. The League's options are all tasks
  and politics.

## The crafts' guilds

$CIVCR00.MSG by day (file 0xA42D5). **verified** (code); see
`CityVisit.cpp`.

- **The card**: the physician (option 0), the alchemists (1), the
  tinkers (3), the clothmakers (4) and the ways out are on; the jewelers
  (2) and the placeholders (5, 6) are off, so the game never offers the
  jewelers from here by day. In cities of size 3 or less the physician is
  missing when (location property 0x21 + year) % 3 is 0, the alchemist
  when property 0x21 is even; in size 4, the alchemist when it is a
  multiple of 3 (see "Names and random numbers").
- Going to a guild (e.g. the tinkers, file 0xA47A2) takes an hour (one
  more if 1367:072A says it is night), unless a random(100) over a chance
  brings an encounter (state 0x3C; not reproduced).
- **The guild shops**: every guild's "buy and sell" opens the trade
  screen, then one hour passes and the guild's card is shown again (the
  swordsmith at file 0xCED6A, 0xD0833 at night; the same for the
  others). The tinkers are the artificers' guild ($ARTIF00, trade at file
  0xD74B9: type 6, mask 0x0000800E), the clothmakers type 8, mask
  0x04000000 (file 0xAD2C9). The night crafts card is $CIVCR01, where
  the tinkers and the clothmakers are off.

## The inn

$URBAN00.MSG by day (file 0xA6B5E, the option switch at file 0xA6DB6,
segment base 0xA6B50), $URBAN01.MSG at night (file 0xA7545). **verified**
(code); see `CityVisit.cpp`.

- **The card**: the options are enabled (1), hidden (0) or disabled (2)
  by words at DS:EE76... A reputation of −40 or less (wanted, hunted)
  shows card 3 ("we have no room") with the meal, the room and the
  storage off. "Recover items" needs location property 0x27 (the
  location record's word +0x18 is not −1, i.e. something is stored).
  The meal is disabled when the purse is under its price, $Money1.
- **The price**, 1462:1D2C (reputation, city size), in pfennigs: p = size
  + 1; p · 4 / 3 at reputation −10 or less, p / 2 at 50 or more, p · 7 /
  10 at 10 or more; at least 1; times the number of party members
  (1462:1DE6); · 8 / 3 if the location's byte +0x14 is 2, · 5 / 3 if it
  is 8 (always 0 in DARKLAND.LOC; not reproduced); clamped to 1..1000.
- **A meal and sleep** (file 0xA6F70, and 0xA790C at night): card 2;
  every member's endurance (attribute 0) is set to its maximum
  (0E76:0B64 reads the maximum, 0E76:0988 sets an attribute) and
  strength gains 1 if under its maximum; the price is paid (1367:023E);
  nine hours pass.
- **The stables** (file 0xA7120): by day card 1 if every member carries
  an item with flag 0x2000 (a mount, 0E76:1326(5), counted by 0E76:0DD8),
  else card 7 ("whether any of your mounts are for sale"); one hour; then
  the trade screen (−1, "Stablemaster", 0x2000). At night (file 0xA7AAE)
  only $URBAN01 card 1, no time.
- **News and rumors** (file 0xA6E40): every member gains an eighth of its
  maximum endurance; a wanted party is found by the guards (card 4, a
  fight) if random(100) is over −10 − reputation; else two hours pass and
  the rumors are shown. Not reproduced (no rumors yet), nor residence,
  storage and the party's composition.

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

## The church

The city church ($CITYC00.MSG by day), segment 1838 (file base
0xB8840): the card at 1838:0000, the options dispatched by a switch at
file 0xB89E0. **verified** (code); see `CityVisit.cpp`.

- **The card**: $Money1 is a tenth of the party's purse, in pfennigs;
  "give $Money1" is disabled when that is under 10. "Seek sanctuary" is
  disabled when the local reputation is over −10 (and a check at
  0x9C0:20F3, not decoded, is false).
- **Mass** (1838:0214): bell b = 1367:07EE; there is a Mass at bell 2
  (Latins) always, at 3 in cities of size 5 or more, 4: 6, 5: 7, 6: 4,
  7: 6, never at 1 and 8. If there is one, every member gains Religion /
  8 + Speak Latin / 35 + 1 divine favor, the party waits until hour
  (b + 1) · 3 (the start of the bell after the next one: from 7 in the
  morning, till noon) and card 2 is shown. Otherwise card 4 names the next
  Mass in $NamedOneName: Vespers (hour 18) in cities over size 3 at bells
  3..5, else Prime (hour 6); no time passes. Afterwards the church's day
  card if the hour is 5..18, else its night card.
- **Confession** (1838:03CC): r = the local reputation; card 3, then 11 −
  r / 10 hours if r ≥ 0, else 12 + r / 20 (divisions truncated). If
  random(100) ≤ the leader's Religion and random(100) ≤ 25, 0x9C0:1F63
  (−1, 9, 1, 10, −1) is called: a routine at 1462:0132 that seems to
  improve Virtue (skill 9), not decoded, not reproduced. The leader gains
  random(5) + Religion / 10 + 2 divine favor.
- **Donation** (1838:067C): the tenth of the purse is taken; points = it
  / (6 · party size). The most religious member gets back all the divine
  favor it lacks, whatever the points; then, while points remain, the
  others in turn, each costing what it lacks. Over 600 pfennigs every
  member gains Religion / 30 Virtue. Card 5 under 120 pfennigs, 6 under
  600, else 7; one hour passes. A bug of the original: 0E76:14A4 returns
  the highest Religion *value*, which the code uses as the member's index
  (so usually a member past the end of the party); the reimplementation
  uses the most religious member.
- The night church ($CITYC01.MSG, code at file 0xB9249) is not decoded.

## Reputation

0E76:1B12 turns a reputation into the index of its word, in the list
"a local hero", "respected", "unknown", "suspected", "wanted", "hunted":
over 50 → 0, over 10 → 1, over −10 → 2, over −40 → 3, over −75 → 4,
else 5. **verified** (code).
