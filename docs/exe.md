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
- DS:9C69 + 128 · slot: a party slot's status byte (0: empty, 1: with
  the party; 2, 3 and 5 are set in a few places, 3 is reset to 0 by the
  passing of time; *inferred*: members away, e.g. captured). The code
  tests it about 115 times, mostly for 0 and 1.
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
  0x2C3EC3FF, stables 0x2000, castle 0x202D, monk 0x0400040C (the Arms
  Outfitter: Soldier's Road, **verified**). Which call is which place is
  otherwise *inferred*: each guild shop is opened twice (by day
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
- **Ctrl+F1..F5** makes that member the leader (file 0x68E44, if the
  member's status byte is not 0), who then bargains; the residence does
  the same (file 0x6FF70, status 1); the map ignores the keys, combat
  refuses them (file 0x4B7E0). No card-screen handler was found.

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

## Game states

The card flows run as states: DS:E48A holds the current one, each
place sets it on entry and loops while it is unchanged (`cmp word
[0xE48A], n`); an option's handler sets the next. Day and night
variants are often n and n + 1, chosen with 1367:072A (`sbb ax, ax;
and ax, 1; add ax, n`: n by day). **verified** (code) for the states
whose loop loads a deck:

| state | deck | state | deck | state | deck |
|---|---|---|---|---|---|
| 0x06 | MAINS01 | 0x1D | URBAN00 | 0x43 | FUGGE00 |
| 0x08 | MAINS02 | 0x1E | URBAN01 | 0x45 | MEDIC00 |
| 0x09 | SIDES00 | 0x1F | DOCKS00 | 0x47 | HANSE00 |
| 0x0A | SIDES01 | 0x21 | CITYG05 | 0x49 | PAWNS00 |
| 0x12 | CITYS00 | 0x23 | SLUMD00 | 0x4B | BLACK00 |
| 0x13 | CHURC00 | 0x25 | CLOTH00 | 0x4F | SWORD00 |
| 0x14 | BUSIN00 | 0x2B | COUNC00 | 0x51 | ARMOR00 |
| 0x15 | MARKE00 | 0x31 | UNIVE00 | 0x55 | BOWYE00 |
| 0x16 | MARKE01 | 0x32 | CATHE00 | 0x57 | ARTIF00 |
| 0x17 | MILCR00 | 0x34 | CITYC00 | 0x59 | ALCHE00 |
| 0x18 | PHYSI00 | 0x3A | SELEC00 | 0x5D | JEWEL00 |
| 0x1A | CIVCR00 | 0x3C | NIGHT00 | 0x6F | CLERI00 |
| 0x1B | CITYF00 | 0x42 | PHARM01 | 0x7C | OTHER00 |

(a night variant is usually its day state + 1; there are over a
hundred states, most of them events and quests). The exits of the places done
so far were checked against their handlers: the guilds' "leave" goes
to the crafts (0x1A) or the arms-making guilds (0x17); the banks'
"return to the marketplace" to 0x15/0x16, their side door and the
League's to the side streets (0x09/0x0A), the League's main door to
the market; the church's, the inn's and the crafts' ways out to the
main (0x06/0x08) and side streets. The church's "talk to a priest"
leads to CLERI00 (0x6F), sanctuary to state 0x81, the inn's "the
composition of your party" to state 0xAA (see "The party's composition").

The options of a state are enabled (1), hidden (0) or disabled (2) by ten
words at DS:EE76..EE88, set when the state starts and changed by its
conditions; the cards have options the game never shows, hidden here too:
the fortress (state 0x1B, file 0xA4B0E) offers the audience, the clerk
and the saint (see "The city's lord") (the dungeon's options, which the card has, stay hidden),
at night (0x1C, file 0xA6240) only its two ways out, the bribes of $Money1
and $Money2 never being offered; the town hall (0x2B, file 0xB1A98) does
not offer the weapons training, and at night (0x2C, file 0xB3446) only
the square and its ways out. **verified** (code)

## The city's lord

The fortress (state 0x1B, file 0xA4B0E, $CITYF00) and the town hall (0x2B,
file 0xB1A98, $COUNC00) have the same three options: an audience with the
lord, a clerk, and a saint's help; the cards have the same numbers in
both decks. **verified** (code)

- **When offered**: the options start dim (2). The fortress turns them on
  unless the city's flag word (record +0x5E) has bit 2 (0E76:1A7E(0x18):
  66 of the 92 cities have it); the town hall unless the party is wanted
  (09C0:20F3), and its clerk is dim again while mark 0x3A runs. The saint
  option (150B:168C) needs a member who knows one of the card's saints:
  Alcuin (5), Raymond Penafort (112), Wolfgang (134), Wenceslaus (129).
  Dim options are hidden here (*inferred*: the card has no dim drawing).
  The town hall's prisoner options (cards 14..17) need an event (mark
  0x4D): not reproduced.
- **The chance** (file 0xA5018, 0xA5300, 0xB223C): 0 with a reputation of
  −40 or less (then the guards' challenge, state 1); else (the leader's
  Speak Common / 2 + the reputation + his Charisma) / 2, 1 while mark
  0x3C runs, 99 with mark 0x3B (nothing makes it), within 0..99.
- **The audience** (file 0xA4D6C; 0xB2038): with mark 0x3A, card 1 and
  the square. Else, random(100) = r: at most the chance: reputation +
  fame / 10 at least r, an hour and card 4, else a wait of min(the
  hours until 18, random(3) + 1) hours, one more in the fortress
  (random(3) + 2 in the town hall: no more), $Number1, card 2; then mark
  0x3C for 168 hours and the lord's offer. Over the chance: mark 0x3C
  for 168 hours; if at most twice the chance, the hours until 19, the
  reputation − 1, card 3 ($ChosenOneName: the leader), the square; else
  the reputation − 10, card 5, the side streets.
- **The clerk** (file 0xA50B8; 0xB2318): the same chance. With mark 0x3A
  card 1 (the main street; the square in the town hall). At most the
  chance: reputation / 3 + fame / 20 at least r, an hour, card 4 and the
  offer (no mark); else two hours, mark 0x3A until 5 o'clock (the
  fortress) or 18 (the town hall), card 7. Over it: mark 0x3C for 168
  hours; at most twice the chance, or a reputation of 10 or more: card 6
  (the main street; the square in the town hall); else the reputation −
  10 (the town hall's not under 0 if it was positive), card 5, the side
  streets.
- **The saint** (file 0xA53A0; 0xB2638): if it answers, Alcuin, Raymond
  and Wolfgang bring a wise old man (card 9, $ChosenTwoName the member
  who prayed), Wenceslaus a regal one (card 10), then card 8, an hour and
  the offer; if not, card 11 and the same place.
- **The offer**: the castle nearest the city, 1462:10B6(10, castle, the
  city's seed + 0x62, 15, 2, 0) (the robber knight, patron 10, level 15),
  an hour, state 0x90 with the square (DS:E7D8 0x12 / 0x19; the main
  street for the fortress's clerk): $RAUBI00 card 1, then card 14, then
  back. In the town hall (file 0xB1D28) first a reward due for the knight
  (0E76:3404(3, 10, location)) pays 7 times the city's size in florins
  and makes a refusal for 72 hours (category 7, subject 10), then the
  thanks (state 0x91, $RAUBI01 card 0, the reputation 40..50 better);
  else, with random(100) under 50, the offer, and otherwise another task
  (state 0x151, not implemented).

## The party's composition

State 0xAA ($PARTY00, 1916:000E at file 0x10D62E; the options through a
switch at file 0x10D8FD), reached from the inn (also at night and when it
turns the party away), and from the barracks' "look for people" (DS:E7D8
0x75); "finish this task" goes back to DS:E7D8. **verified** (code) unless
marked.

- **The options** (flags DS:EE76..EE88): finding somebody is shown unless
  the party has 4 members or more (dim, 2); "allow $ChosenOneName to
  retire" and the three after it are shown for the members in slots 0..3
  when the party has two or more (and, for slots 1..3, who are not
  followers: 0E76:38D2(slot, 0x43), an event); the four "ask $ChosenTwoName
  to leave" (slots 1..4) are for the followers. A member in the fifth slot
  cannot retire. $ChosenOneName..$ChosenFiveName are the slots' nicknames.
  Followers, who the quests bring, are not in this game yet.
- **Retiring** (file 0x10DB9C...; 0x10D98C(slot)): card 7, an hour; the
  purse (DS:906B) and the letters of credit (DS:9072) each lose a fifth
  (1367:00F2, 0180), the card says the member takes a fifth of "the party's
  entire wealth"; 09C0:1EC3 = 1462:3B58 of overlay 0x25 (file 0x72230) takes
  him out of the party. A follower leaves with card 9 and nothing more
  (event 0x43 deleted, 09C0:18B5).
- **Looking for people** (file 0x10DA7A): random(3) + 2 hours ($Number1),
  card 6, then 0E76:2246(location) = 1462:0000 of overlay 0x26, the
  character selection screen (not decoded: cards 8 and 9 and PARTY00's
  other texts belong to it and to the followers). Card 8 says the person
  chosen "pawned all useful equipment" and has no cash.
- **Reproduced** (`CityVisit`): the screen, the retiring, and, in place of
  the selection screen, a list of the members who retired in this city
  (*inferred*: the card says they "might be available to rejoin"). They
  are kept in memory only: a saved game does not hold them, so they are
  lost when the game is saved and loaded, or closed. New people to hire
  are not made (without any, "not implemented").

## The cathedral

By day state 0x32 ($CATHE00, file 0xB6A28; the handlers through a switch
at file 0xB6CF0, the preconditions of each option in a table at DS:EA14,
base 0xB6A20), at night 0x33 ($CATHE01, file 0xB7960). **verified** (code)

- **The options**: dim until set: the gift needs a purse of 7200
  pfennigs (30 florins; $Money1 is a third of the purse, 6A1:2D28 is a
  signed long division); the patron saint (option 3) needs the
  location's word +0x1A (property 0x26) not 0, and the relic from the
  canon (4) +0x1C (property 0x25) not 0 and no task of the bishop's: both
  words are 0 in the game data and the saved games (but one record), so
  neither is ever offered; giving a relic needs an item of flag 0x1000
  (0E76:0DD8(−2, 0x1000, 0)); the sanctuary as for the church; the
  bishop's reward needs one due (kinds 3 or 10, patron 3: never).
- **Mass** (file 0xB6D74; night 0xB7BA2): by day at bell b = 1367:07EE:
  bell 2 always, 3 in cities of size 6 or more, 4: 7, 5: 8, 6: 5, 7: 7,
  never at 1 and 8; at night only at bell 7 from size 7. Each member
  gains divine favor Religion / 60 + Speak Latin / 40 + 1 (0E76:0A72), the
  party waits until hour (b + 1) · 3, card 2. Else card 1 names the next
  Mass (1FB8(0, hour, 3)): 18 (Vespers) by day at bells 3..5 above size 3,
  1 (Matins) at night at bell 1 from size 8, else 6 (Prime).
- **A priest** (file 0xB6F34; 0xB7D30): an hour. By day, with unrest here
  (an event of kind 2: 0E76:3470) and rebels (0E76:3742(2, 0, location))
  the priests judge whether the party is worth their superior's ear
  (card 20, state 0x6F) if random(100) is under the chance the game
  passes, which is the stub's −1 (the table's second entry is the same
  stub as the Mass's): never, card 21. Else the prelate speaks: card 3,
  4, 5 by the relics' word and 6, 7, 8 with a chapel, none of which the
  data have: card 3.
- **A gift** (file 0xB71DC): a third of the purse; the leader's divine
  favor + gift / 200 within 0..30 and a lesson in Virtue
  (09C0:1F63(−1, 9, 7, 10, 0)); an hour; card 9 under 240 pfennigs, 10
  under 1440, 11 under 3600, else 12.
- **Giving a relic** (file 0xB768E; 0xB7F20): the reputation + 30
  (0E76:19D0(location, 30, 30)), every member's divine favor + 99, the
  relic is left (09C0:203F finds it, 2035 removes it; $NamedOneName is
  its name, $ChosenOneName the leader), three hours, card 18 (night 10).
- Not reproduced: the politics (0x6F, $CLERI00), the patron saint, the
  relic from the canon and the bishop's task, the reward.

## Sanctuary

State 0x81 ($SANCT00, 187B:0000 of overlay 0x4C, file 0xF5EF0; the church's
day card 8, the night card 4 and the cathedral lead to it; the monastery
refuses it). The option is offered unless the party is not wanted
(09C0:20F3) and the local reputation (0E76:199C) is over −10. The card is
always 0; $NamedOneName is the captain of the guard, 1367:0DB4(the city's
seed + 0x156). Options (a switch at file 0xF6040). **verified** (code)

- *rest till nightfall* (by day) and *till daybreak* (at night): the hours
  until 19 or 6 (1367:086A); the other one is then offered.
- *the captain's word* (0xF62E2): random(3) hours; with the reputation
  over −10 card 1, over −75 card 2, else card 3.
- *sneak out* (0xF636C; the chance 0xF641E): the lowest sum of a member's
  Stealth and Streetwise (0E76:1446(15, 16)), within 10..99, he being
  $ChosenOneName. random(100) at most it: card 10, two hours, the side
  streets; else card 9 and an hour.
- *give yourself up* (0xF6154): with the reputation over −75 card 11 and the
  dungeon (state 0xD); else a battle (type 0x18, seed location + 0x6F,
  random(5) + 4 of enemy 3 at variant random(3) + |s| / 4 + 1 and one
  sergeant, as the guardroom's). Its result (0xF61F3): the reputation
  −1..−5 (0E76:19D0); 0 or 1 the guards nervous (mark 0x12) for 2000 /
  size hours, an hour, the churches (0x13); 2 an hour, 0E76:23E2 (not
  decoded) and the sanctuary again; 3 or 4 three hours and the dungeon.
  Then mark 0x11 (wanted) for 240 hours if the reputation was −75 or
  less, else 120.
- Cards 4..8 and 12 are never shown by this code.

## Names and random numbers

- 06A1:29A4 `srand`, 06A1:29B6 `rand` (Microsoft C: seed = seed ·
  0x343FD + 0x269EC3, returns bits 16..30); 0410:0008 `random(n)` =
  (rand() · 2 · n) >> 16.
- 1367:0DB4 (seed, kind, slot) names a person: srand(DS:9C4A + seed),
  random(1000) thrown away, then by kind, e.g. kind 0: a man's first name
  (108, the far pointers at 290E:235F), a space, a surname (146, at
  290E:267F); women's names (88) are at 290E:2517. Then srand(time).
  DS:9C4A is the game's seed global: a new game takes it from the BIOS
  clock ticks (0040:006C, file 0x7C04F), a saved game keeps it at 0x64
  (see formats.md). `ExeData.cpp` reads the lists from the executable.
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
- **Students** (file 0xA349C): card 6 ("nothing I can teach you") if the
  best healer's Healing is over the skill and the skill is over 1; card
  11 while a mark (kind 0x38, 30 hours) says he refused; if (P + year)
  % 3 (signed) is not 0 and the skill is over 1, card 4: students for
  skill / 5 + 10 pfennigs a day ($Money1), a person of kind 0x28 being
  made if there is none (0E76:392C, 2C4E: skill Healing, fee 60 at
  +0x1E, level 50 at +0x20, for 168 hours: the residence charges the
  60, not the card's fee);
  else card 5, $Number1 = random(4) + 1 apprentices, and the mark. The
  lessons are given while the party lives at the inn (the inn's
  residence, not decoded).
- **At night** (outside 1367:072A's day, hours 5..18; DS:8DFA): card 1,
  without the discussion and the students, with "apologizes profusely,
  gives him two groschen" (file 0xA39B6: 2 groschen paid, then state
  0x14). Leaving without them (file 0xA3902): if random(100) <= 50 the
  local reputation falls by random(4) + 1 (0E76:1DFE) and card 7 is
  shown. State 0x14, where the two groschen, card 3 and the idiot lead,
  is the district ($BUSIN00: its loop tests it at file 0x9F166); the
  other departures (0x1A) lead to the crafts.
- Not reproduced: the lessons.

## The market at night and the night watch

$MARKE01.MSG (state 0x16, file 0xA0C62) and $NIGHT00.MSG (state 0x3C,
file 0xBF0BB, where the walks at night end when their chance fails,
e.g. the crafts' (file 0xA47C5)). **verified** (code); see
`CityVisit.cpp`. "Marks" are timed events per location (0E76:2930 makes
one for n hours, 0E76:2A32 adds hours to it).

- **The market**: card 19 while mark 0x17 (watched) runs, else card 0.
  Sneaking needs no mark 0x1A; bribing ($Money1 = max(4, size −
  reputation / 10) · party size · 24 pf, at least 48) needs the purse
  and no mark 0x19. Potions and saints are offered when the party has
  them (0x150B:16E8, 168C).
- **Sneaking** (file 0xA0F96): chance (file 0xA109A) from 100, each
  member in turn lowers it to his Stealth if lower, then adds 30; − 20
  while watched or when 0x9C0:2107 says so. Success: a lesson in Stealth
  for everyone (1462:0132(−2, 15, 1, 10): 15 % each) and the offices
  (state 0x101, $INSID00: burglary, not implemented). Failure (the same
  roll): if under twice the chance and 95, card 2, mark 0x17 + 72 hours,
  an hour, the side streets; else card 1, mark 0x1A 12 hours, 0x17 + 32,
  and the watch (then the offices).
- **Bribing** (file 0xA10FC): taken if the reputation is over −10 (and
  0x9C0:20F3 is not 1): paid, card 9, the offices; else card 10, mark
  0x17 + 72 hours, the watch.
- **The watch**: card 1 after the market or the grove at night (state
  0x16, 0x22), card 2 if mark 0x40 runs ("Not you again"), else card 0;
  mark 0x40 for 7 hours. The fine ($Money1): (city size − reputation /
  50 + the purse's florins + 1) · party size (0x9C0:2161) pfennigs; paying
  it (file 0xBF5C0) takes an hour and returns to the state the caller
  left in DS:E7D8. Running (file 0xBF61A): if random(100) <= (the slowest
  member's speed, 0E76:0656: agility lowered by the load, + the best
  Streetwise) / 2: the reputation may fall by 1 (0E76:19D0: if
  random(100) <= 100 − |reputation|), a small lesson in Streetwise, an
  hour, card 3, the side streets; else card 4 (the slowest has fallen
  behind: $ChosenOneName), without running.
- **Walking at night** (e.g. the crafts' streets, file 0xA4596 and the
  handlers at 0xA455A...): outside the game's day, the watch stops the
  party (state 0x3C, returning to the crafts, DS:E7D8 = 0x1A) if
  random(100) is over clamp(1, 99, the party's average Stealth
  (0E76:1600) + the best Streetwise − 1462:0000(15, 1, 5)). That
  hazard, 1462:0000(a, b, low) of overlay 0x27 (file 0x809A0; b is not
  used), is clamp(low, a, random(a − low)), a raised by the location's
  state (property 0x20, its byte +0x14: 1 · 5 / 4, 2 · 6 / 4) and by each
  of marks 0x13 and 0x12 (· 6 / 5): 5..9 here. **verified** (code)
  The tinkers' and clothmakers' streets cost their extra night hour
  first. About 35 handlers use this check (the guilds at night, the
  jewelers, other places): only the crafts' are reproduced.
- **Attacking the watch** (the fifth option; the options' actions go
  through a switch at file 0xBF30E: 0 pay 0xBF5C0, 1 run 0xBF61A, 2
  potion 0xBF71A, 3 saint 0xBF81E, 4 fight 0xBF914; the table at DS:EA14,
  segment 1838 at file 0xBF0B0, holds their help texts, e.g. "Enter
  Combat"): they flee (card 7, then on as after the fine) if
  random(100) <= c, c = |reputation| / 10 + the party's average Charisma
  / 10 (0E76:1800(5): the average of an attribute) + the leader's best
  weapon skill / 2 (0E76:01C0(−1)) + the fame / 20 (0E76:1326(4)), 0
  under 75, at most 90 (1367:0066). Else (file 0xBF3A2) a battle:
  0E76:2278 gets the battlefield type (0x13, 0x11, 0x15 or 0x56 by the
  state the party came from), a seed (that state + the location) and
  the foes, **the same as TAC.TXT prints**: ftype1 3, fqual1 1, fnum1 die(5)
  + 3 (4..8 of enemy 3, "Guard", at variant 1), ftype2 0, fqual2 2, fnum2
  1 (a "Sergeant" at variant 2), ftype3 −1: so ftype is an enemy of
  DARKLAND.ENM and fqual the variant of its group. 0E76:19D0(location,
  −15, −25) lowers the reputation by 15 + random(10) with a chance of
  100 − |reputation| %. The battle's result (file 0xBF440): 0 card 8
  ("the unconscious and bleeding night watch", then on as before; and
  0E76:2C4E(−2, location, 0x1E, 0x12, ...), not decoded), 1 card 9, 2
  card 10 (a retreat), 3 card 11 (the dungeon). **verified** (code).
  Reproduced with BattleView; the map among ICITY.000..003 and the
  starting places are not the game's (not decoded), a retreat is Esc
  and leads to the side streets (inferred); for the dungeon see "The
  dungeon, the magistrate and the execution".
- Not reproduced: the burglary, potions, saints, the load's effect on
  speed.

## Arriving at a city

The game never uses $OUTSI00.MSG (its name is nowhere in DARKLAND.EXE);
coming from the map the party stands before the walls. **verified**
(code; the deck names in DGROUP are "$CityE00", "$CityG01"...).

- **Before the walls** (state 4, $CITYE00, file 0x941C0): card 5 for
  a capital, 0 for a city ruled for its lord, 6 for a free city, by the
  city record's +0x58 (+0x5A while the location's flags have bit 0x80;
  0E76:1A7E(0x28)). The options (actions at file 0x94398...): the gate
  by day (at night card 1 and a wait until 7 o'clock, 1367:0716), the
  gate at night (by day card 2 and a wait until 20 o'clock), the wall
  by day (card 3, city size / 2 + 1 hours, state 0xE) or at night (card
  4, size / 2 + 2 hours, state 0xF), travel elsewhere.
- **Card variables** (the switch at file 0x8E170, segment 150B at
  0x8D110, on the index of the name in the table at 290E:21FF):
  $CityLordName is the city record's place 0, the ruler (case 50);
  $CityLordTitle (case 51) the ruler in a capital (place 1 while the
  flag 0x80 is set), else, by the record's +0x56 (the city's number),
  one of "Vogt", "Erbvogt", "Obervogt", "Burggraf", "Richter",
  "Landhofmeister" (290E:2323, number % 6) where the city is ruled, one
  of "alte Herr", "Ältere Herren", "Frager", "Losunger", "alte
  Losunger", "Oberste Hauptmänn", "Schultheiss", "Schöff", "Bürgermeister"
  (290E:233B, number % 9) in a free city; $PlaceAttitude (case 16) the
  word of the local reputation (290E:2183, 0E76:1B12). **verified**
  (code)
- **The gate by day** (state 2, $CITYG01, file 0x925B0; card 18 while
  mark 0x12, nervous guards): the toll ($Money1) is (size / 3 + 1)
  pfennigs per member; the options' actions go through a switch at file
  0x9284D, their chances through the table at DS:EA14 (segment 19DD at
  file 0x925B0):
  - *pay* (file 0x928B8; not offered without the toll): if random(100)
    <= c, with c 0 for the wanted (mark 0x11) or a reputation of −10 or
    less, 100 for a reputation of 0 or more, else 100 + the reputation
    (· 2 while mark 0x12) within 1..99 (file 0x92916): the toll, an
    hour, card 1, the main street (state 6 or 8); else state 1, $CHALL00
    ("They're wanted here -- arrest them all!"), no time;
  - *befriend the guards* (file 0x9298A; not offered while marks 0x0A
    or 0x11): state 1 for the wanted or disliked (−10 or less); c =
    the reputation / 2 + the leader's Charisma or Speak Common,
    whichever is higher (1367:0084, a max), within 1..99 (file
    0x92A62): card 2, a lesson in Speak Common for the leader (1462:0132
    mode 1), the reputation + 1 (0E76:19D0), two hours, the main street;
    else mark 0x0A for 12 hours, card 3, a lesson of mode 0, an hour,
    back to the gate;
  - *sneak in with the crowd* (file 0x92ADE; not offered while mark
    0x0B): c = (the average speed, 0E76:060E, + the average Streetwise,
    0E76:1600) / 2, halved for the wanted, within 1..99 (file 0x92B8A):
    card 4, a lesson in Streetwise for all (1462:0132(−2, 16, 1, 10)),
    an hour, the main street; else mark 0x0B for 12 hours, a lesson of
    mode 0, and card 5 back before the walls, or for the wanted an hour
    and state 1;
  - potion (file 0x92BCC), attack (0x92E1C): not decoded; saint
    (0x92D40): see "Saints"; *reconsider* (0x93052): before the walls, no time.
  09C0:20F3 (1462:00BA), which also stops the befriending, is not
  decoded (taken as false).
- **The gate at night** (state 3, $CITYG00, file 0x93448; the actions
  through a switch at file 0x93660): the bribe ($Money1) is (size / 3 +
  1) · the party's size (DS:A67E, the last used slot + 1) · 18 / 10
  pfennigs, or (100 − reputation) / 33 pfennigs for a negative
  reputation (so, oddly, far less); not offered without it.
  - *rely on your fame* (file 0x936C0; not offered while mark 0x0C):
    with a reputation of −10 or less card 3 ("I recognize you"), the
    first three options gone for this stay, no time; else if random(100)
    < the reputation / 2 + the fame within 0..100 (file 0x937A8,
    1367:0028 clamps to 0..100; at least 0): card 1, no time, the main
    street (state 8); else mark 0x0C for 12 hours, card 2 ("Nobody
    through the gates till dawn"), an hour, the gate (state 2 or 3 by
    the hour);
  - *talk your way inside* (file 0x9380A; mark 0x0D): card 3 as above
    with a reputation of −40 or less; else if random(100) < (the
    leader's Speak Common + 2 · Intelligence) / 2 (file 0x938EE): card
    4, a lesson in Speak Common for the leader (1462:0132 with mode 7),
    an hour, the side streets (state 9 or 10); else mark 0x0D for 12
    hours, card 2, a lesson of mode 0, an hour, the gate;
  - *bribe* (file 0x93948): card 3 with a reputation of −10 or less;
    else paid, card 5, an hour, the side streets;
  - potion (file 0x939EC): not decoded; saint (0x93B24): see "Saints";
    *fall back*
    (0x93BF2): card 12 of one of two decks at random, an hour, before
    the walls.
- **The wall by day** (state 0xE, $CITYW00, file 0x99AEC; actions
  through a switch at file 0x99DD9): the bribe is (size / 3 + 1) · the
  party's size · 14 / 10 pfennigs, or (100 − reputation) / 33 for a
  negative reputation; not offered without it. "Climb with a rope" is
  offered when the party has item 59, the rope (0E76:0C76(−2, 0x3B)
  counts it by the item code), "everybody climbs" when it has none and
  nobody has status 2 or 3 (0E76:1972); mark 0x0F takes the climbs away.
  The climber ($ChosenOneName) is the member with the best (2 · speed,
  0E76:06DE, + Stealth) / 2.
  - *bribe* (file 0x99E76): paid, card 1, an hour, the side streets;
  - *the rope* (file 0x99F0E): if random(100) < the climber's score:
    card 3, a lesson in Stealth for him (1462:0132 mode 7), an hour, the
    side streets; else card 4, a fall (1462:026A), a lesson of mode 0,
    an hour, and both climbs gone for this stay (the handler's options),
    the wall again (state 0xE or 0xF by the hour);
  - *everybody climbs* (file 0x9A024; chance: the weakest member's
    score, file 0x9A210): random(100) < it, card 11, a lesson in Stealth
    for all, an hour, the side streets; else every member whose score
    is at most that roll falls (card 12 with his name, a fall, a lesson
    of mode 2), the others a lesson of mode 5 and card 13 each (back down
    to help), an hour, both climbs gone;
  - potion (file 0x9A272): not decoded; saint (0x9A392): see "Saints";
    *fall back*
    (0x9A476): before the walls, no time.
- **A fall** (1462:026A(member, 0, 1, 10), the training overlay at file
  0x809A0): Strength loses random(10 · Strength / 40 + 1) − 1, at least
  0; Endurance random(10 · Endurance / 20 + 1) − 1, at least that and at
  least 1; neither more than the attribute's maximum (0E76:0B64); the
  attribute is changed by 0E76:0A72, so it stays at 1 at least.
- **The wall at night** (state 0xF, $CITYW01, file 0x9A7E0; switch at
  0x9AA35): the rope needs item 59, everybody climbs needs 0E76:1972,
  the sewer grate is taken away by mark 0x10. The climber is chosen
  from the second member on, the first being the default.
  - *the rope* (file 0x9AAC4) and *everybody climbs* (0x9ABD4): as by
    day with cards 1, 2 and 3, 11, 12;
  - *the sewer grate* (file 0x9AE3A): the strongest member
    ($ChosenOneName, 0E76:05EE(m, 1)) loses 3 Endurance; if random(100)
    < 16 · his Strength / 10 (file 0x9AF58): card 4, a positive local
    reputation falls by 2..4 (0E76:19D0(−2, −4)), size / 3 hours, the
    side streets; else 0E76:2D5C(member, location, 0, 0x10, 3, 99, 0,
    500, 0, 0): mark 0x10 for 500 hours (the eighth argument is the
    length, as 24 for mark 0x0F and 36 for 0x12 elsewhere), card 5, an
    hour, the wall;
  - potion (0x9AFAE): not decoded; saint (0x9B0C4): see "Saints";
    *fall back*
    (0x9B1D6): before the walls, no time.
- Reproduced in `CityVisit`: before the walls, the waits, the gates and
  the walls but for their potion and attack options, and the
  guards' challenge below. Not
  reproduced: the lessons of modes 0, 2, 5 (TrainSkill() has mode 1),
  the cards of every fallen and climber after a failed climb (one of
  each is shown), the statuses of 0E76:1972. The speed is the agility (the load is not kept).
- **The guards' challenge** (state 1, $CHALL00, file 0x914FE; the
  options' actions through a switch at file 0x91744, their chances in
  the table at DS:EA14, segment 18D1 at file 0x914F0): an hour passes
  on arrival; DS:A88D, the state the party came from, decides where it
  goes back. **verified** (code)
  - the bribe ($Money1, file 0x9156C) is city size / 2 florins, +
    |reputation / 20| for a negative reputation, at least one; offered
    only if the purse holds it and the party came from the side streets
    (states 9, 10);
  - *fight* (file 0x917B0, the battle at 0x92370): the battlefield type
    by the previous state as for the watch, a seed (that state + the
    location), clamp(the party's size (09C0:2161, 1462:1DE6), 7,
    random(7) + 2) of enemy 3 at variant 1 and one of enemy 0 at
    variant 2, the reputation −15..−24 (0E76:19D0(location, −15, −25)).
    The result (file 0x9241E): 0 card 1, mark 0x12 for 2000 / size
    hours, no time, back (state 0xC after state 0x3A); 1 (0E76:23E2)
    card 2, mark 0x12, an hour, the side streets (state 9 or 10 by
    1367:072A; 0xC after 0x3A); 2 (0E76:23E2) card 3, an hour, back; 4
    card 4, three hours, state 0xD (the dungeon). Then mark 0x11 (wanted)
    for 120 hours, 240 when the reputation on arrival was −75 or less;
  - *run* (file 0x917EE): an hour, state 0x7A (the chase, below);
  - *talk* (file 0x91838): if random(100) <= clamp(0, 100, the leader's
    Speak Common + Charisma + the reputation, − 25 while mark 0x13)
    (file 0x91978): a lesson in Speak Common for the leader (09C0:1F63,
    mode 1), card 5, 6 or 7 by random(3) (card 7 names a weapon of the
    leader through 09C0:1E9B and 0E76:1FB8, not decoded; when that is 0,
    card 6 or 5), an hour, back; else card 8, a lesson of mode 0 and the
    fight;
  - *bribe* (file 0x919E0): c = clamp(0, 100, the reputation + the
    leader's Charisma + the bribe in groschen) (file 0x91A6C, 1367:00F2
    and a division by 12); **if random(100) < c** the guards refuse
    (card 10, the fight), else card 9, an hour, back. Neither takes the
    money. Both look like slips of the original, reproduced as they are;
  - potion (0x91AC2): not decoded; saint (0x91C62): see "Saints";
    *surrender* (file
    0x91E44): card 4, three hours, state 0xD.
  Reproduced in `CityVisit` from the day gate's failures, with
  BattleView: a won battle is result 0, a retreat (Esc) result 2, a
  lost one result 4; result 1 has no BattleView counterpart. Card 7
  names the weapon in the leader's hand (*inferred*), cards 5 or 6
  without one. Not reproduced: potions, the lesson of mode 0.

## The dungeon, the magistrate and the execution

The state table (DS far pointers at file 0x18C700, one per state, to
RTLink thunks) gives the handlers: state 0xD at 1852:0006 of overlay
0x2F (segment 1852 at file 0x988C0), 0x8C at 1838:0000 of overlay 0x4F
(file 0xFB4A0), 0x8D in the same overlay (file 0xFBEA8, segment 18D8).
**verified** (code)

- **The dungeon** (state 0xD, $DUNGE00, file 0x988C6; actions through a
  switch at file 0x98B9A, chances in the table at DS:EA14): on entry
  the search (18E7:0854(-2), file 0x66484): the purse is emptied, and
  each item goes if its quantity · weight is over 2, else if random(100)
  is under h / 2 (for 2) or h (less), h = (Agility + Stealth) / 2 of its
  owner (so the nimbler lose more, as the code has it). DS:8DEE is the
  cell (card 0 the best, 1 the dark one, 2 the oubliette, 3 the dark
  one in Saint Lucy's light), DS:8DEC the tunnel in %; both are kept
  (DS:E3D0, E3D2) while the priest (state 0x83, $DUNGE01) visits. The
  cards of the worse cells have placeholders for the options they lack;
  "pick the lock" needs item 64, the lockpicks (0E76:0C76(-2, 0x40)),
  "seduce" a woman standing (0E76:007C: status 1 and the member's byte
  +3). Mark 0x4D with fewer than four members brings card 18 (a friend
  already in prison joins; not reproduced).
  - *pick the lock* (file 0x98C1E): the picker is the best at Artifice
    (0E76:14A4(14)); c = his Artifice, + 50 in cell 1, − 50 in cell 3,
    0 in cell 2, within 0..99 (1367:000A) (file 0x98D36); if random(100)
    <= c: a lesson (mode 1), an hour, a dagger (item 7) for every member
    (18E7:0128(-2, 7)), card 6 and the guardroom; else card 5, a beating
    (1462:026A(-2, 0, 1, 20): a fall of amount 20 for all), three hours,
    the lockpicks taken (18E7:0668(-2, 64)), a lesson of mode 0, cell 1
    from cell 0, else 2, the tunnel lost;
  - *the guardroom* (file 0x999C2): battlefield type 8, the seed
    location + 0x6F, random(5) + 4 of enemy 3 at variant random(3) + |s|
    / 4 + 1, one of enemy 0 at variant random(3) + 1; s is 09C0:1C1B
    (1462:0470), a measure of the party (the average best weapon skill +
    0E76:026A + the average Virtue and Alchemy, / 37; not reproduced).
    Results 0..2: two hours, card 8, the chase (state 0x7A); 3, 4:
    0E76:23E2, two hours, card 9, the search again, a beating, the same
    cell;
  - *climb to the window* (file 0x98DAA): an hour; the climber
    (0E76:179C(2, 1)): a member whose Agility + Strength is over the
    best score so far becomes the climber and the score becomes twice
    his Agility; c = score / 3 within 0..100 (file 0x98E86); success:
    card 10, a lesson in Stealth (mode 1), a club (item 15) each, the
    chase; else card 11, a worse cell;
  - *dig* (file 0x98EBE): 12 hours; after 12 o'clock (DS:00E0) one time
    in nine (random(9) == 1) card 16 and the magistrate (state 0x8C);
    else the tunnel + random(6) + 12 (+ 50 in cell 3): over 95 card 12,
    the reputation −1..−6 (0E76:19D0), the side streets; else if
    random(100) < tunnel / 4, card 13, a beating, the tunnel lost, the
    cell + 1 (from 3 to 2); else card 22 ($Number1: the tunnel);
  - *seduce the turnkey* (file 0x9909A): the woman with the best
    Charisma (0E76:174A(5)); if random(100) <= her Charisma: 48 hours,
    card 14, a lesson in Speak Common, her Virtue − 0E76:18A8(Virtue, 2,
    6) (2, or 2 + random(4) if random(100) <= |100 − Virtue|), the side
    streets; else two hours, card 15, a lesson of mode 0;
  - *ask for a priest* (file 0x991CA): three hours, state 0x83 (the
    cell and the tunnel kept in DS:E3D0, E3D2; the priest, below);
  - *pray* (file 0x99220): every member's divine favor + random(10) + 2
    (+ 12 in cell 3), card 17, 12 hours, then after 12 o'clock one time
    in ten the magistrate;
  - *wait* (file 0x995A2): until 13 o'clock (1367:0716), then the
    magistrate if random(100) <= 11;
  - saint (file 0x9932A): see "Saints"; acid (0x99638: an Eater Water
    used, clubs, card 24, the guardroom): not reproduced.
- **The magistrate** (state 0x8C, $MAGIS00, file 0xFB4A0): card 1
  instead of 0 after a torture (DS:8E14, reset on entry). Pleading
  and confessing call 0E76:3CDE(0x11, ...) (the wanted mark lifted,
  *inferred*). 1838:0A04 (file 0xFBEA4) returns 0.
  - *say nothing* (file 0xFB61C): under three tortures, card 1, six
    hours, every member loses random(18) Endurance and random(12)
    Strength, the leader's Endurance at least 1; else card 4, an hour,
    the square (state 0x12 by day, 0x19 at night);
  - *plead innocence* (file 0xFB74C): s = random(5) + reputation / 40
    − 3: under −6 or over 1 card 9 (acquitted), an hour, the square; −6..−1
    card 5, three hours, the execution (state 0x8D); 0 card 6, three
    hours, a flogging (as the torture), the square; 1 the fine;
  - *confess* (file 0xFBB02): s = random(3) + reputation / 40 − 2: the
    same, but −5 and below or over 5 does nothing (never happens);
  - *the fine* (file 0xFB85A): random(3) + size / 3 florins, at least
    one ($Money1); with that many florins in the purse, 24 hours and
    card 7, else three hours, card 8 and a flogging; the square. The
    purse is not touched in either case (as the code has it).
  - saint (file 0xFB978): see "Saints".
- **The execution** (state 0x8D, $EXECU01, file 0xFBEA8):
  - *refuse to struggle* (file 0xFC57A) is 1838:0CF6(−1) (file 0xFC196),
    the rescues, each tried with random(100) (random(50) for argument
    0x50), then an hour: the reputation / 10 or less, card 7, the
    pardon (reputation set to −9, the square); the best Virtue +
    Religion + Charisma + Speak Latin / 10 (added to 0; with an event
    of kind 2 here, 0E76:360C, to an uninitialized word), card 8, the
    abbot (the church,
    state 0x34 or 0x35); the florins in the purse, card 9, the bankers
    (five hours, reputation −9, the square; the bank is not touched);
    |reputation / 5|, card 10, the mob (a falchion, mace or short spear
    each by the best weapon skill, 0E76:01C0, and the fight); two more
    on DS:9082 / 10 (state 0x84, the ruler's quest, and a case 5). Else
    (file 0xFC4CE) a member at random is beheaded (card 1, 09C0:18B5,
    status 0) and the execution goes on;
  - *break the ropes* (file 0xFC5BC): if random(100) <= the strongest's
    Strength (0E76:16FE(1)), card 6, a dagger each, the fight; else card
    11 (a member at random) and the rescues;
  - *the fight* (file 0xFC004): battlefield 0x1B, random(4) + s / 3 + 1
    of enemy 3 at variant random(2) + |s| / 4 + 1, enemy 23
    ("Executioner") at variant s % 3 + 1; the reputation − 5..19
    (0E76:1DFE sets location property 0, the reputation). Results
    0..2: 0E76:23E2, card 12, 0E76:2C4E(...), an hour, the chase; 3, 4:
    card 13, three hours, the execution again. Then mark 0x11 for 240
    hours (480 at −75 or less; 0E76:2930).
  - saint (file 0xFC6B2): see "Saints"; the rescues take the saint
    who answered as their argument (random(50) for St. Jude, 0x50) and
    when none comes it decides: St. Alcuin (5) the pardon or the abbot
    (random(2)), St. John Nepomuk (0x4E) the pardon, St. Jude any of
    the six (random(6)); cases 4 and 5 of the switch (file 0xFC339,
    table at 0xFC33E) are the city ruler's quest, state 0x84.
- Reproduced in `CityVisit`, from the arrests (the gate's challenge,
  the night watch, the chase). Not reproduced: potions, the
  lessons of mode 0, s (taken as 0), the rescues on DS:9082, card 18.
- **The priest** (state 0x83, $DUNGE01, 1901:000C of overlay 0x4C,
  segment 1901 at file 0xF6750; the handler at file 0xF675C, actions
  through a switch at file 0xF686A). Back to state 0xD, the dungeon's
  entry searches the party again (its call to 18E7:0854 does not
  depend on the state it comes from); the cell and the tunnel are
  restored. **verified** (code)
  - *confess* (file 0xF68B4): a lesson in Virtue for the leader
    (09C0:1F63(−1, 9, 1, 10)), his divine favor + random(10) + the
    reputation / 20 (0E76:0A72(leader, 6, ...)), three hours, card 1;
  - *help to escape* (file 0xF6970): c = the leader's (Virtue +
    Religion + Charisma + Speak Latin) / 10 + 5 · 1901:05CE() ·
    1901:05CE() (a function returning 0) + 20 while bit 0x40 of
    location property 0x20 (the location's byte +0x14) is set; 0 if not
    over 0, else within 1..99 (file 0xF6A86). If random(100) <= c, 24
    hours, then over 25 card 2 and the church (state 0x34 or 0x35),
    else the leader gets lockpicks (item 64) or, when random(2) is not
    1, an Eater Water (item 99), $NamedOneName, card 3, the dungeon;
    else an hour, card 4 and the saved cell becomes 2 from 0, 1 from the
    others (the reverse of the dungeon's own rule);
  - *a good word* (file 0xF6B2C): a score is computed ((Religion + Speak
    Latin + Virtue) / 10 + reputation) and never used; card 5, 12 hours,
    after 12 o'clock one time in ten card 6 and the magistrate, else
    the dungeon;
  - *leave* (file 0xF6C64): the dungeon.
  Reproduced in `CityVisit` but for bit 0x40 (not kept: 0).

## Quests

The game makes its quests as events (formats.md "Events") when a
patron gives them. **verified** (code) unless marked.

- **Making an event** (0E76:2C4E(a0..a9), file 0x5A4FE): a free slot
  of the 300, filled by file 0x5A8E4: +0x1A = a0, +0x1C = a1 (the
  place), +0x20 = a2, +0x22 = a3 (category), +0x00 = a4 (subject),
  +0x1E = a5, +0x26 = a6, +0x24 = a8, +0x28 = a9 (kind), +0x2A..+0x2E
  0; created and started now; the end a7 hours later (1367:09EA), or
  never for 9999 (31/12/1499 23h). 0E76:3B62(category, subject, place,
  +0x1A, kind, +0x2A), −1 for any, finds one; 0E76:353E(kind, +0x1A,
  +0x1E): one of category 8 started; 0E76:3404(kind, +0x1A, place): one
  of category 36 (a reward due) started; 0E76:392C(category, subject,
  place): one started (unless DS:00F0).
- **The banks' tasks** (the Fuggers, file 0xC473E; the Medici, 0xC677E;
  the patron 8 or 6, its seed the city's number + 8 or + 6): offered
  (file 0xC42C3) unless a task of theirs of kind 10 or 3 runs from here
  (0E76:353E(k, patron, location)), a refusal lately (0E76:392C(7,
  patron, location)), a reputation or standing under 0, or a reward due
  (0E76:3404: the reward's option instead). The chance (file 0xC48EE,
  0xC68F4): the reputation + the fame (0E76:1326(4)) + the location's
  property 0x0C (the Fuggers) or the Medici's standing (DS:4BB8),
  within 10..50. If random(100) is at most it: one time in two
  (random(100) < 50) the robber knight: $Money1 twice the city's size
  in florins, card 1, the castle nearest the city (1462:2842(x, y, 2):
  the nearest location of type 2 not on the city, without bit 4 of its
  byte +0x14, by the octile distance), 1462:10B6 (below), an hour,
  state 0x90 with DS:E896 the castle and DS:E7D8 the bank; else the
  city's size in florins, card 1, an hour, state 0x151 (another quest,
  1462:1712: not decoded). Else card 10 and an event of category 7,
  subject the patron, +0x20 0x5F, 72 hours (the refusal).
- **The robber knight's task** (1462:10B6(patron, castle, seed, level,
  strength, extra), overlay 0x27, file 0x81A56): an event of category
  8, kind 3 at the castle (+0x1A the patron, subject its seed, +0x1E
  the city, +0x20 0x5F, 9998 hours); if the castle has no knight yet
  (0E76:3B62(8, −1, castle, −1, 3, −1)) it is his: +0x2A the level
  (12 for the banks), +0x2C the strength (at most 4), +0x2E; with his
  men (category 28, kind 3, subjects 2, 3, 4, 9998 hours) and the
  castle taken (category 43, +0x1E 1, forever); else the knight's
  event keeps the higher level and strength. The saved games' robber
  knight at Grötsch has exactly this shape.
- **The offer** (state 0x90, $RAUBI00, 1838:0000 of overlay 0x50 at
  file 0xFE800): the knight's event at DS:E896; $NamedOneName the
  patron (1367:0DB4(its subject)), $NamedTwoName the knight
  (1367:0DB4(castle + 1100, 0, 1)); the card by the patron's place
  (DS:E7D8): 6 the Fuggers, 8 the Medici, 7 the League, 5 the
  alchemist, 3 or 4 the market's merchants (+0x1A 4: the foreign
  traders), 0 the cathedral's bishop, 2 a village's Schulz, else 1 the
  city's ruler; then card 14, where the castle is ($Direction2 from
  the city nearest it, $NearestCity, $Direction from here). From a
  village or the League with four members or fewer, card 10 or 13
  offers a companion (0E76:4650); else back to the patron.
- **The tower** (state 0x93, $RAUBI03, segment 1929 at file 0xFF710):
  every castle's arrival; $NamedOneName the knight (1367:0DB4(place +
  1100, 0, 0)); card 24 (a rude fort) instead of 0 with an event of
  kind 3, subject 0x27 here (0E76:360C(3, 0x27, place)). Its options
  (a switch at file 0xFF940): lay siege, alchemy, ask to come inside
  (off while mark 0x26), single combat, sneak in after dark, a saint,
  storm it (offered with an event of kind 3, subject 5 or 0x27 here:
  allies), go away (state 0xC). Every option but "go away" sets mark
  0x26 for 6480 hours. **verified** (code) s below is the garrison's
  losses, +0x1E of the knight's event of subject 2 here (0E76:3742(3,
  2, place), 0 without one).
  - *lay siege* (file 0xFFD5A): random(24) hours, card 1; then the
    knight attacks if random(3) + 2 <= s (card 7, the knight's
    battle), else his band returns if random(11) <= s + 5 (card 8),
    else a sally (card 9), both the men's battle;
  - *ask to come inside* (file 0x10019E): s one less with a fame over
    200, one more over 400; r = random(6); if s <= 0 or r > s + 4 card
    11, an hour, the audience (state 0x95, $RAUBI05); if r <= s card
    12, an hour, the men's battle; else card 10, three hours, the tower;
  - *single combat* (file 0x100316): if random(100) <= s · 4 − fame /
    10 + 50 (within 1..99) card 13, two hours, the knight's battle;
    else if random(4) <= s card 14, an hour, the men's battle; else
    card 10, three hours;
  - *sneak in after dark* (file 0x100486): two hours, by day until 19h;
    the chance (the average Agility + the average Stealth) / 2, + 30
    with an event of kind 3, subject 4 here (0E76:360C), within 0..99:
    card 16, a lesson in Stealth of mode 1 for all, inside (state 0x94,
    $RAUBI04); else card 17, a lesson of mode 0, the men's battle;
  - *a saint* (file 0x1005C4): Edward the Confessor, Eric, Hedwig
    (card 15, an hour, the audience) or Reinold (card 18, a lesson in
    Stealth of mode 1, an hour, inside); unanswered, card 19, an hour,
    the tower;
  - *storm it* (file 0x100722): an event of subject 6 for 3 hours
    (0E76:2C4E(−2, place, 0, 0x1C, 6, 0, 0, 3, 0, 3)), inside.
- **The tower's battles**: the knight's (file 0xFF9B8, battlefield
  0x75): enemy 35 (the Raubritter) at variant 4, one. Won, card 20
  and his end; fled, card 21, two hours, the tower; lost, card 22, a
  day, a beating (a fall of 20 for all), the search (18E7:0854) and
  s one less (0E76:2E74(..., add −1)), the map. The men's (file
  0xFFBF6): random(5) + 3 of enemy 3 at variant 2. Won, card 23, an
  hour, s one more (0E76:2E74(−2, place, 0x1C, 2, 1, 3, add), made for
  999 hours if missing), the tower; fled, card 21, three hours, the
  map; lost, card 22, three hours, the map.
- **The knight's end** (file 0xFFA7F): an event of category 28,
  subject 1, +0x20 0x5B for 2400 hours; 0E76:3E06(3, place): each
  category-8 event of kind 3 here becomes a reward due (category 36,
  8760 hours, at its +0x1E, +0x1A and subject the patron, +0x1E its
  subject, +0x20 and +0x2E kept, +0x2C the place); the patron's city's
  reputation rises by its +0x2A, the fame by 1462:1E06(its +0x2C) (0,
  3, 10, 25, 64, else 200; 2/3 of it at difficulty 0, 3/2 at 2); if
  there was any, the events of kind 3 here of categories 8 and 28 are
  gone (0E76:3D92), the one just made too. Then mark 0x27 for 800
  hours; the castle's flag 4 (+0x14) and state 0x157.
- **The banks' reward** (the fourth option, offered with a reward due:
  0E76:3404(3, patron, location); the Fuggers at file 0xC4940, the
  Medici at 0xC692E): the bank's standing + 5 (DS:4BB6, 4BB8); with
  +0x2E 0, twice the city's size in florins (1367:0130), $Money1, card 7,
  DS:E7D8 the bank (0x43, 0x45), an event of category 7 and subject the
  patron for 72 hours (no new task meanwhile), state 0x91; with an item
  (+0x2E, its code), card 8 names it ($NamedTwoName) and nothing more
  happens (the item is not given, the reward stays). A reward of the
  other task (kind 10): the city's size in florins, card 7, state
  0x152. Without a reward: the standing − 3, card 9, the market.
- **The patron's thanks** (state 0x91, $RAUBI01, 1870:000C of overlay
  0x50 at file 0xFEB8C): the reward (0E76:3B62(36, −1, location, DS:E896
  the patron, 3, −1)) names the patron (1367:0DB4(its +0x1E)) and the
  knight (1367:0DB4(its +0x2C + 1100, 0, 1)) and is deleted; the card by
  DS:E7D8: the square and the places 0x19, 0x1B, 0x1C card 0 with the
  reputation + 40..50, the market card 2, the Hanse (0x47) card 6 or 7,
  the alchemist (0x59) card 4, 0x3F card 3, else (the banks) card 5 one
  time in four (random(4) = 0), else 8; then back to DS:E7D8.
- Reproduced in `CityVisit`: the banks' tasks, the robber knight's
  offer, the tower's options, battles and the knight's end (at the
  chosen difficulty: the fame of a task is 2/3 at basic and 3/2 at
  expert), the banks' reward and thanks. Not reproduced: the
  other task (state 0x151), the other patrons, the companions, the
  alchemy, the audience (0x95), inside the tower (0x94), the ruin
  (0x157), the banks' standing.

## Leaving the city

- **The gate from inside** (state 0x3A, $SELEC00, 191A:0004 of overlay
  0x3A, segment 191A at file 0xBC8C0; actions through a switch at file
  0xBCA48): the main streets (and others) come here day and night, and
  the handler always shows card 0: cards 13..16 ("The gate is closed
  for the night...", no card call shows them) are never used.
  **verified** (code) State 0xC is out of the city (the map).
  - *walk out* (file 0xBCD30): an hour, out; after a fight at the gate
    (mark 0x13) when random(100) < 40, or when 09C0:20F3 (not decoded,
    taken as false), card 1 and the fight;
  - *hide among the people* (file 0xBCDDE): c = the average Agility +
    Streetwise of the members standing, − 25 while mark 0x13, within
    0..100 (file 0xBCEBE); if random(100) <= c card 2, a lesson in
    Streetwise for all (09C0:1F63(−2, 16, 1, 5)), an hour, out; else a
    lesson of mode 0 (09C0:20F3: state 1), an hour, card 1, the
    reputation −1..−4, the gate;
  - *the fight* (file 0xBCAB4 after card 1, 0xBD27A for "attack"):
    battlefield 0x2B, the seed location + 0x70; random(4) + |s| / 4 + 3
    of enemy 3 at variant |s| / 4 + 1 and the sergeant at random(3) + 1
    (s: 09C0:1C1B); after card 1 while mark 0x12, 7 at variant 5 and
    the sergeant at 5. The reputation − 40, or at −40 or less −
    0E76:18A8(reputation, 3, 9) (0E76:19B4 sets it). Mark 0x12 for 36
    hours if the result is under 2, mark 0x13 for 24 (0E76:2D5C). The
    result: 0, 1 mark 0x12 for 120 hours (0E76:2B36), card 9, an hour,
    out; 2 card 10, an hour, the gate; 3 card 11, the gate; 4 card 12,
    three hours, the dungeon. Then mark 0x11 for 120 hours, 240 at −75
    or less;
  - potion (file 0xBCF36): not decoded; saint (0xBD13E; Christina,
    Lutgardis, Milburga, file 0xBC94B): card 7, an hour, ±2..8, out; no
    answer card 8, an hour, the gate; *the wall* (0xBD4FC): state 0x3B,
    no time; *not yet* (0xBD53C): the previous state (DS:A88D).
- **The wall from inside** (state 0x3B, $SELEC01, 1A1F:0006, segment
  1A1F at file 0xBD910; actions at file 0xBDBF7): reached from the gate,
  the side streets (by day no time, file 0x96D18; at night card 1 of
  $SIDES01 and an hour, 0x975FE) and the crafts district (an hour,
  0x9F588). The options (file 0xBD992): those with the horses are
  offered when the party has one (an item with flag 0x2000, 0E76:0DD8),
  the others when it has none; mark 0x10 takes away the sewers, 0x22
  the sally port, 0x0F the sally port, the ropes and the first climb;
  no rope (item 59), the ropes; a purse under the bribe takes away the
  second sewer option (as the code has it). The bribe ($Money1): (size
  / 3 + 1) · the party's size · 2 pfennigs, or (100 − reputation) / 33
  for a negative reputation, twice that while mark 0x13. **verified**
  (code)
  - *the sewer* (file 0xBDC88, 0xBDE1A): the member with the best
    Agility + Strength ($ChosenOneName), c = that · 8 / 10 within 0..99
    (file 0xBDD9E); random(100) < c: card 1, the horses left (09C0:202B
    (−2, 0x2000, 0)), out; else mark 0x10 for 500 hours (0E76:2C4E), a
    positive reputation −2..−4, size / 3 hours, card 2;
  - *the sally port* (file 0xBDF9A): reputation over −10 and not wanted
    (mark 0x11): the bribe paid, card 3, an hour, the horses left, out;
    else card 4, mark 0x22 for 48 hours, state 1 (the challenge);
  - *the rope* (file 0xBE0B8; 0xBE1EC with the horses: by day card 12
    and a wait until 19 o'clock): c = the lowest Stealth of the party
    (0E76:1396(15)), halved while mark 0x13 (file 0xBE1A8); random(100)
    <= c: a rope used (18E7:04D4(−2, 59)), a lesson in Stealth for all
    (mode 7), card 5, the horses left, three hours, out; else card 6,
    an hour, mark 0x0F for 24 hours;
  - *over the wall* (file 0xBE35A; 0xBE562 with the horses, the wait as
    above): the same c; the roll r = random(100) <= c: a lesson in
    Stealth for all, every member whose speed · 3 (0E76:06DE) is under
    r falls (1462:026A(m, 0, 0, 5)), card 7 (none), 8 (one) or 9, the
    horses left, three hours, out; else card 6, an hour, a lesson of
    mode 0, mark 0x0F for 24 hours. The options without the horses do
    not wait for the dark, although their text says so;
  - *a gate instead* (file 0xBE7B6): an hour, state 0x3A; saint
    (0xBE800; Christina, Lutgardis, Milburga): card 10, an hour, out; no
    answer card 11, an hour; *the streets* (0xBE928): the side streets.
- Reproduced in `CityVisit`. A lost battle at the gate is result 3 (the
  surrender, result 4, has no BattleView outcome); the speed is the
  agility; 09C0:20F3 and the potions are not reproduced; card 13 of
  $SELEC01 (St. Reinold) is never shown by this code.
- **The square at night** (state 0x19, $CITYS01, 18FC:0008 of the overlay
  at file 0xA3A60: a handler and a chance for each option, 0xA3C4A): the
  chance (0xA3E48, 0xA3EF2, 0xA3F94, 0xA403E, 0xA40F0, 0xA41A8, 0xA4260)
  starts at 100; each member in turn brings it down to his Stealth if
  lower, then adds 25 (the notices), 28 (the town hall), 20 (the prison),
  30 (the barracks), 50 (the university), 45 (the main street) or 65 (the
  side street); 20 less (30 the prison, 35 the barracks) if mark 0x14 runs
  or 09C0:2107 (1462:0102: 2 with mark 0x12, + 1 with 0x13) is exactly 1;
  within 0..99 (1367:000A). If random(100) is at most the chance: the
  notices (state 0x6D, which end at the square, DS:E896 = 0x19; no time),
  the town hall (0x2C, no time), the prison (0x11E, not decoded), the
  barracks (0x75, the same code by day, no time), the university (0x31),
  the main street (6 / 8) or the side street (9 / 0xA), the last three
  after an hour (1367:05C8(1)). Else 0E76:2D5C(−2, location, 0, 0x14, 1,
  99, 0, 32, 0, 0), mark 0x14 for 32 hours, and the watch (0x3C), which
  returns to 0x19. **verified** (code). The slum from the business
  district takes no hours and has no chance, day or night (0x9F49C).
  The prison's state 0x11E (also the town hall's prisoner options) is
  1901:000A of overlay 0x4C: file 0xF675A, a single `retf`: the game has
  no code for visiting a prisoner there, so it stays "not implemented".
- **The boats** (state 0x1F, $DOCKS00, file 0xA7DE6; options through a
  switch at file 0xA831C): the city record's words +0x4A, +0x4C, +0x4E and
  +0x50 are up to four destinations (−1: none), each shown as dim (a boat
  that leaves later in the week) or on (today): with the city's flags
  (property 0x20) bit 1 on, a boat leaves today when (the city's seed + the
  day of the month + its number) % 4 = 0 and the fares are doubled
  (1367:0290); with bit 2, one boat at random and the fares times four;
  else % 3 = 0. The card: 1 with bit 1; with bit 2 card 4 while mark 0x39
  runs, else card 6 (empty piers) when (seed + day) % 3 = 0, else card 2
  (the officer, who sells passage with the 0x5935 / 0x5942 files...); else
  0. An hour first. Boarding (file 0xA83E4): a wanted party may be caught
  (state 1); too poor, card 7 (with mounts) or 8, an hour; else the fare is
  paid, random(3) + 1 hours (card 9, $Number1), the party is at the
  destination (DS:907E), 48..95 more hours pass (random(48) + 48) and the
  voyage begins (state 0x5A, overlay 0x42, with the decks $BALTI00/01,
  $RIVER00...): not reproduced.
- **The docks at night** (state 0x20, $Docks01, file 0xA9820; its options
  are the handlers of a table at file 0xA99F2): option 0 (the boats) goes
  to the docks' day state 0x1F; options 3 and 4 the main and side streets
  (states 6 / 8 and 9 / 0xA). Options 1 and 2 are the same code, file
  0xA9ABC: the first is shown when the party has mounts (0E76:1326(5)),
  the second when not, and from June to October only (the month, DS:00E4,
  0-based, over 4 and under 10; else card 1, the ice, with both blank):
  the horses (09C0:202B(−2, 0x2000, 0)), all armor (flags 0x40 and
  0x04000000) and each other item but three in ten (09C0:2021(−2, 30):
  18E7:0AB8, random(100) over 30 loses a whole stack) are left; random(4)
  + 4 hours pass ($Number1). The weakest member's Strength
  (0E76:164A(1)) at 10 or more: card 2, everyone ashore. Else each member
  in turn: with a Strength under 10 and random(100) over it, card 3 and
  the member leaves the party (09C0:18B5); else card 4 for the first one
  ashore ($ChosenOneName), card 5 for the others ($ChosenTwoName, after
  $ChosenOneName). Then state 0xC, the map. **verified** (code). The
  original removes the member at once, which shifts the party while the
  loop goes on; here the lost leave when the cards are over.
  Reproduced in `CityVisit`; not the boats.

## Saints

- **The rules** (**verified**, code; `ExeData::Saints()`, `darklands
  --saints`): a far pointer per saint at 290E:2937 (136) to an RTLink
  thunk into segment 165C of overlay 0x27 (file 0x82940, the overlay of
  1462 at 0x809A0). Each function takes (member, value, mode): modes
  0..6 return a word of a table it fills on the stack: 0 flags (bits
  1 and 2 tested by the saint screen), 1 a kind (0..9), 2 the divine
  favor it costs, 3 a number 50..99 (not known; not used by mode 7), 4
  the Virtue it needs, 5 the chance's base, 6 always 1; mode 7 is the
  chance, 165C:0000(base, Virtue, needed Virtue, value, cost, 0) (file
  0x82940): 0 if the Virtue is under the needed one or the value under
  the cost, else base + (Virtue − needed) / 2 − cost + 0 + value; the
  caller passes the cost as the value when the member's divine favor
  (attribute 6) is at least the cost, else 0, so the chance is base +
  (Virtue − needed) / 2, or 0. Mode 8 is the saint's own effect (e.g.
  St. Adrian: attributes and all weapon skills up for a while,
  0E76:4C04, 4B08), different for each saint.
- **A card's saints**: each state's handler puts up to four saints in
  DS:EE4B (−1: none; its potions in DS:EE43). The saint option
  (150B:168C) is available when a member standing knows one of them
  (0E76:1260). Choosing it (file 0x8E252) replaces the card's text
  with a line per member and per card's saint he knows; the line's
  value gives the member (DS:EE6C) and the saint's place in the list
  (DS:EE6A), and the handler's saint function calls 0E76:2180(member,
  saint) = 1462:0000(member, saint, 1) of overlay 0x22 (file 0x6B7D0):
  the saint's screen (file 0x6B9FC: its picture and description from
  DARKLAND.SNT, where one may give up: −1), then if random(100) <= the
  chance, mode 8 and the divine favor − the cost (1); else the favor −
  the cost, and − half of it more if random(99) < chance − 66 (0).
- **DARKLAND.SNT**: a count byte (136) and 136 records of 360 bytes,
  the saints' descriptions (NUL-padded). **verified** (1 + 136 · 360 =
  48961, the file's size)
- Reproduced in `CityVisit`: the option, the list (with a line to give
  up), the chance and the favor. Not reproduced: the saint's screen,
  the saints' own effects (mode 8), the meaning of mode 3. The
  dungeon's saints (file 0x9932A; list at file 0x9896E): St.
  Bathildis (card 7: half the purse, the square), St. Dismas and St.
  Peter (card 19, the reputation −1..−8, the square), then by the cell
  St. Reinold (card 20, the side streets), St. Lucy (card 3, the lit
  cell; nothing in it already), St. Jude (card 21, the reputation
  +2..+7, the square); three hours; no answer, card 23 and three hours
  more.
- **The other cards' saints** (their lists: file offsets of the
  `mov word [0xEE4B], ...`; the functions; "±a..b": 0E76:19D0 with
  +a..+b while the reputation is over −10, −a..−b else):
  - the guards' challenge (0x9163D: Christina, Genevieve, Godfrey,
    Reinold; 0x91C62): an hour; Christina card 16, ±3..9, state 0xC
    (away from the city: back to the map here); Genevieve, Godfrey card
    17 ($NamedOneName), +2..6, back where the party was; Reinold card
    18, ±2..6, the side streets; no answer: card 15 and the fight;
  - the gate by day (0x9273D: Lutgardis; 0x92D40): card 10 (card 11 for
    a second saint, never listed), ±4..12 (−3..−9), the main street; no
    answer card 12, the gate. At night (0x9357D: Lutgardis, Milburga;
    0x93B24): card 10, ±2..6, the main street; no answer card 11, an
    hour, the gate by the hour;
  - the wall by day (0x99CAB: Lutgardis, Milburga; 0x9A392): card 9,
    +1..4, the side streets; no answer card 10, the wall. At night
    (0x9A902: Christina, Lutgardis, Milburga; 0x9B0C4): card 8
    (Christina) or 9, an hour, the side streets; no answer card 10, an
    hour, the wall by the hour;
  - the night watch (0xBF1D6: Raphael, Finbar, Lucy, Odilia; 0xBF81E):
    an hour, +5..10, card 6, on as after the fine (DS:E7D8); no answer
    card 13 in place of the watch's card (DS:8E04), its saint option
    gone (DS:EE7C = 2);
  - the magistrate (0xFB518: Devota, Lawrence; 0xFB978): card 2, every
    member loses random(20) Endurance (the leader keeps 1), 12 hours,
    card 4 and the square; no answer card 3;
  - the execution (0xFBF17: Alcuin, Gregory Thaumaturgus, John Nepomuk,
    Jude; 0xFC6B2): Gregory card 3, three hours, the side streets; the
    others card 2 and the rescues (above); no answer card 4 and the
    rescues without a saint;
  - the market at night (0xA0DAD: Christina, Dismas, Gregory, Jude;
    0xA1410): the offices (state 0x101), not reproduced.

## The chase

State 0x7A ($CHASE00, 1995:0002 of overlay 0x4A, segment 1995 at file
0xF2110; the handler at file 0xF2112, actions through a switch at file
0xF2270, their values in the table at DS:EA14). It follows running from
the gate's guards, the escapes through the window, the guardroom and
the execution's fight. No time passes on entry. **verified** (code)

Three of its options compare random(100) with their value the other
way round from the rest of the game: **the party fails if random(100)
is under the value**, so the better the party, the likelier the
failure. Reproduced as the code has it.

- *keep running* (file 0xF22D0): the value is three times the slowest
  member's speed (0E76:0656, file 0xF238A); success: random(4) hours,
  card 15, a lesson in Streetwise for all (09C0:1F63(−2, 16, 1, 5)), the
  side streets (state 9 or 10); failure: card 14, an hour, the fight;
- *fight* (file 0xF23B2): the fight;
- *ambush* (file 0xF23F0): the value is the leader's Streetwise +
  Stealth within 0..100 (file 0xF249C); success: lessons in Stealth and
  Streetwise for the leader (mode 1, 7), card 12; failure: lessons of
  mode 0 and card 11 (the hiding's "sneeze", with the leader, as the
  code has it; card 13, the flopped ambush, is never shown); the fight
  either way;
- *hide* (file 0xF24E4): the value is the lowest Stealth of the party
  (from 99), + 20 outside the game's day, + 3 · the city's size, within
  0..100 (file 0xF25C4); success: a lesson in Stealth for all (mode 1,
  10), by day a wait until 19 o'clock and card 9, at night until 5 and
  card 10 (1367:0716), the side streets; failure: card 11 (the leader
  sneezes), a lesson of mode 0, the fight;
- potion (file 0xF2632): not decoded; *surrender* (file 0xF27AA): card
  4, three hours, the dungeon (state 0xD);
- *the fight* (file 0xF2A40): battlefield type 0x1A, the seed (previous
  state + location), random(5) + 3 of enemy 3 at variant 2 and random(4)
  + 3 of enemy 0 at variant 2; the reputation −1..−5 (0E76:19D0). The
  result: 0 card 1, 0E76:2D5C(−2, location, 0x1E, 0x12, 1, 99, 0, 2000
  / size, ...), no time; 1 card 2, the same, an hour; 2 card 3, an hour
  (both after 0E76:23E2); all three the side streets; 3, 4 card 4, three
  hours, the dungeon. Then mark 0x11 (0E76:2930) for 120 hours, 240 when
  the reputation on entry was −75 or less.

Reproduced in `CityVisit`: a won battle is result 0 (with mark 0x12 for
2000 / size hours, *inferred* from 0E76:2D5C's arguments), a retreat
result 2, a lost one result 4. Not reproduced: the potion, the lessons
of mode 0.
  A weapon given in the dungeon is taken in hand when the hand is empty
  (*inferred*).

## The alchemist

$ALCHE00.MSG (state 0x59, file 0xD9B53; reached from the crafts in an
hour, file 0xA4614). **verified** (code); see `CityVisit.cpp`. P is the
city's property 0x21 (its number + the seed global).

- **Entry** (file 0xD9E30): card 6 while he is offended (a mark of kind
  0x44, 60 hours); card 2 if (seed global + location) % 30 is not under
  clamp(10, 99, the best Alchemy); card 3 outside the game's day; card 0,
  then card 1 for the next questions. Card 6, 2 and 3 lead back to the
  crafts. $NamedOneName is 1367:0DB4(P + 0x49), $ChosenOneName the best
  alchemist (0E76:14A4(7)).
- **His skill**: city size · 3 + (P + 4) % 41, + (P + 9) % 11 + 10 in a
  city with flag 0x100 (property 0x1B). Under 25: no formulas for sale,
  $Text4 is "Alchemical Components", else "Potions".
- **The chance** (file 0xD9F6C): clamp(0, 99, leader's charisma +
  reputation / 10 + P % 31 + leader's Speak Common / 3 + best Alchemy /
  2 − 15); both options below fail if random(100) is over it: card 6,
  the 60-hour mark, the crafts.
- **A better stone** (file 0xD9FFE, once a visit, if the purse holds
  $Money1): quality (P + 4) % 25 + 1; if the party's stone is worse, it
  becomes that, paid clamp(1, 50 + (seed + location) % 20, (skill / 2 +
  1) · (quality + 5) / 12) groschen (card 7), else card 5.
- **Purchasing** (file 0xDA0CA): the trade screen ("Alchemist", type
  −1), potions (mask 0x800) if his skill without the bonus is over 24,
  else components (0x400).
- Not reproduced: the formulas, their trade, the instruction, the tasks.

## The inn's cache

"Store some items with the innkeeper" and "recover items stored here"
(file 0xA720C, 0xA72B8) show $URBAN00/01 card 5 or 6, open the same
screen (0E76:225C → 0x9C0:1ED7; file 0x6DAC0...: BUYSELL.PIC), then
enable "recover" (location property 0x27: the location record's word
+0x18 is not −1, the location's cache) and let an hour pass.
**verified** (code); see `TradeView.cpp` (cache mode).

- **The screen**: "Get an item from cache", "Put an item into cache",
  "Cache another person's items", "Leave"; the scrolls "The cache
  contains..." and "%s currently has...", rows "%Fs  (%3d) %2d-Qual".
- **The cache** (DS 2E38:37F4, count DS:89F6): entries of 4 bytes,
  item code, quality, count. Putting (file 0x6E086, 0x6E682) moves one
  piece: an entry with the same code and quality counts one more, else a
  new one is added; getting (file 0x6E1D6) gives the member one piece
  (0x9C0:1F6D, with the definition's type and weight) and counts one
  less, the entry going at 0 (file 0x6E6FA).
- **CACHE.TMP** (file 0x6E900): a word per cache, the offset of its data
  (the cache number's word at 2 · number); there, a count byte and the
  4-byte entries. The game's file is 198 bytes, all caches empty.
- Not reproduced: CACHE.TMP (the caches are kept for the session), the
  deterioration card 6 speaks of (not found in this code).

## The residence

"Take up residence to study, work, pray, experiment" at the inn (file
0xA709A) calls 0x9C0:1EEB, which runs the camp screen (file 0x6F97C...;
CAMPCITY.PIC in a city, CAMPWILD.PIC in the wilderness: its argument, 0
at the inn, 1 in the wilderness, 2 and 3 other camps). **verified**
(code); see `ResidenceView.cpp`.

- **Activities**, per member (DS:8A18, their values DS:89FE; keys J R P
  A E G T, 1..5 for the member, F1..F6 the information screens (file
  0x6FF0A), Ctrl+F1..F5 the leader, S, L): relax (0); regain
  strength (1, when strength is under its maximum: gain clamp(1, 99,
  the party's best Healing / 15)); pray (2, when divine favor is under
  its maximum: gain (Religion + Virtue) / 12 + 1); alchemy (3, making
  potions: not decoded); earn money (4, in a city); guard the camp (5, in
  the wilderness); train or study (6, when the location has a person of
  kind 0x28). With the mouse on "Train or study" (file 0x6F520) the
  teachers are listed over the menu (file 0x70DB4: their skills'
  names, x 245..317 from y 48, 9 high, DS:8A32); a click chooses one
  (DS:89FA, file 0x6F365), whose skill the member then studies at its
  fee (DS:89FE, the text "%dpfs", file 0x6FC03). The menu (file 0x7001C) is drawn light green (10) where
  the activity makes sense, green (2) elsewhere.
- **Jobs** (file 0x70A0E): the 31 records of 18 bytes at DS:3ACE (name
  index into 290E:219B, city flags mask, a mask that makes the job never
  offered, location flags mask, an attribute and threshold, two skills
  and thresholds, a multiplier); pay = ((the city record's +0x56 % 4) +
  min(0, reputation / 20) + (attribute − threshold) / 3 + f(skill −
  threshold) for both skills) · max(city size, 9) · multiplier / 70,
  with f(x) = 1 under 4, else clamp(1, 9, isqrt(x)) (file 0x70BDC,
  0x47C:11AB). The best paid job wins; under 2, "Day Laborer" at 2 pf.
- **A day's price** (file 0x708E8): the inn's price (1462:1D2C), less
  the workers' pay, plus the fee (+0x1E) of each student's teacher. The
  text (file 0x70160) shows the inn's price and the purse after the day.
- **A day** (file 0x7050A): refused ("Not Enough Money") if the purse is
  short of a positive price; relaxing and regaining strength restore
  endurance; regaining strength and praying return to relaxing at the
  maximum; alchemy returns to relaxing; the pay goes to the purse; a
  student pays the fee if the purse can, then 1462:0132(member, the
  teacher's skill, 1, level · 20 / 100, −1): a point if random(100) <=
  amount · 33 / 10 (0E76:18A8 gives 0 or 1, raised to 1); then the inn
  is paid if the purse can; then AddHours up to 5 in the morning, at
  least 9 hours (1367:086A(5), + 24 under 9).
- 1462:0132 is at file 0x80AD2: segment 1462 is shared by overlays too
  (0x9C0:1EEB's 1462:0082 is in another one).

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
- **Names**: the master banker's is 1367:0DB4 with the city record's
  word +0x56 + 8 (the Fuggers, file 0xC4280), + 6 (the Medici, file
  0xC62E9); the League's master's + 7 (file 0xC7BBF). They are
  $NamedOneName in the cards of the tasks and rewards.
- Not reproduced: the standings (not in the saved games), the rewards
  and politics. The tasks: see "Quests". The League's options are all tasks
  and politics.

## The crafts' guilds

$CIVCR00.MSG by day (file 0xA42D5). **verified** (code); see
`CityVisit.cpp`.

- **The card**: the physician (option 0), the alchemists (1), the
  tinkers (3), the clothmakers (4) and the ways out are on; the jewelers
  (2) and the placeholders (5, 6) are off, so the game never offers the
  jewelers from here by day. Their handler exists (file 0xA46CE: an hour,
  then state 0x5D/0x5E, $JEWEL00/01) and no other code enters those
  states: the jewelers seem unreachable in the released game. In cities of size 3 or less the physician is
  missing when (location property 0x21 + year) % 3 is 0, the alchemist
  when property 0x21 is even; in size 4, the alchemist when it is a
  multiple of 3 (see "Names and random numbers").
- Going to a guild (e.g. the tinkers, file 0xA47A2) takes an hour (one
  more if 1367:072A says it is night), unless a random(100) over a chance
  brings an encounter (state 0x3C; not reproduced).
- **Soldier's Road** (the arms-making guilds' first option, state 0x17,
  file 0xA217E): card 1 or 2 of $MILCR00 (by the city's property 0x21
  odd or even, 150B:03B0(card, 1): variants of the crafts' own card,
  not reproduced), the trade screen as the "Arms Outfitter" (0E76:21AA(1,
  "Arms Outfitter", 0x040000FF): the goods merchant's quality, the
  arms and armor), two hours, back to the crafts. **verified** (code)
  The street's own cards, $SOLDI00 and $SOLDI01 (states 0x4D and 0x4E,
  segment 190C of overlay 0x3F at file 0xCD790: the trade, then the
  crafts; the leader's secrets are placeholders; "leave" goes to state
  4, before the walls), are never entered: no code sets those states.
- **The guild shops**: every guild's "buy and sell" opens the trade
  screen, then one hour passes and the guild's card is shown again (the
  swordsmith at file 0xCED6A, 0xD0833 at night; the same for the
  others). The tinkers are the artificers' guild ($ARTIF00, trade at file
  0xD74B9: type 6, mask 0x0000800E), the clothmakers type 8, mask
  0x04000000 (file 0xAD2C9).
- **At night** the game shows the same card 0 (0x150B:049A(0), file
  0xA4478): $CIVCR00's card 1 and $CIVCR01 are never used (no code
  refers to $CivCr01). The places themselves change at night.

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
- **News and rumors** (file 0xA6E40, 0xA78B7 at night): every member
  gains an eighth of its maximum endurance; at −40 or less the guards
  come (card 4, then state 1, the challenge) if random(100) is over −10
  − reputation (file 0xA6F26); else two hours, and the news (state 0x66,
  "News and rumors" below), back to the inn after (DS:E7D8). Not
  reproduced: the party's composition.

## Encounters in the city

The code of the city's places, followed from the streets, the square,
the market, the slum and the grove, has two random encounters besides
the night watch: the thieves in the slum and the shell game. The grove
(states 0x21, 0x22, $CITYG05/06) has none: an hour (card 1), a bell
(card 2, three hours) or until nightfall (card 3, then card 4 with
$Number1 the hours when 8 or more) by day; at night an hour, a bell, or
until 5 in the morning (card 3, 0E76:19D0(location, −1, −1), card 5
with $Number1 the hours, then the grove by day). **verified** (code)
Reproduced in `CityVisit`; the grove's day is the game's, the hours
5..18 (1367:072A), as everywhere in the cities.
The grove's options 3..7 (file 0xAA654.., "...3".."...7" on the cards)
are placeholders, hidden as on the other cards (the day's 5..7 would go
to state 0x62).

- **Waiting in the grove** costs no money, draws no encounter and
  has no exit with a risk: the exits lead straight to the streets. The
  Darklands wiki ("Scenic Grove") agrees: "waiting around is safe and
  costs nothing", a risk of "a bandit or night watch encounter when
  approaching the grove at night, but not when leaving" (the bandits
  being the watch, see below). It finds no evidence that camping lowers
  the local reputation, but the code has it: the night camp until
  morning (not the day's waits) asks 0E76:19D0(location, −1, −1), which
  lowers it by 1 with a chance of 100 − |reputation| %. What the wiki
  says of rest also follows from the passing of time (0E76:255A, see
  "Time and travel"): endurance comes back to its maximum − 4 and no
  more, strength only at the turn of a day and by chance (the party's
  best Healing against random(150)); the message of waking "as if you'd
  had a regular night's sleep" (card 4) shows after 8 hours or more of
  the day's nap. **verified** (code)

- **The way to the grove**: the risk is on the way there, in the
  streets' handlers. Each street draws a hazard h on arrival
  (1462:0000, see "Walking at night"; 0 when it does not apply), and
  its grove option fails if random(100) >= 100 − h:
  - the main street by day (file 0x956CC, h = 1462:0000(35, 3, 20) in
    DS:57B0) and the side streets by day (file 0x96BCF, h(15, 2, 2) in
    DS:57C8), only for a wanted party (09C0:20F3 = 1462:00BA: mark
    0x11, a local reputation of −75 or less, or of −10 or less with
    mark 0x13): the guards' challenge (state 1);
  - the main street at night (file 0x96296, h(14, 1, 4) in DS:57BC)
    and the side streets at night (file 0x974B6, an hour first, h(8, 1,
    1) in DS:57D4): the watch (state 0x3C, card 1), returning to the
    grove (DS:E7D8 = 0x21/0x22);
  - the crafts' streets (file 0x9F43C): no risk.
  None of them leads to the thieves (state 0x24 is set only in the slum
  and in the thieves' own code), so the "bandits" of the grove are the
  watch or the guards. **verified** (code). Reproduced in `CityVisit`
  (the hazard is drawn when the option is taken).

- **The slum** (state 0x23, segment 19A2 of overlay 0x34 at file
  0xAAEC0; the options through a switch at file 0xAB07A): *rest* (file
  0xAB104) is card 5 and an hour. *Live very cheaply* (file 0xAB49E):
  city size / 3 hours, card 2 where the city's property 0x21 is odd,
  else card 3, a lesson in Streetwise of mode 7 for all (09C0:1F63(−2,
  16, 7, 10, 0)), then the camp screen of type 3 (0E76:222A(3), see
  "The residence"): a day costs one pfennig (file 0x7092B), and before
  each day (the S key, file 0x6FDBE) if random(100) is at least
  clamp(25, 95, the best Streetwise + 25) the camp ends with mark 0x52
  for 2 hours. After it, with mark 0x52: random(2) + 2 hours, card 7,
  DS:E7D8 = 0x23 and the thieves (state 0x24); with mark 0x51 (an
  alchemy accident) state 0xB5; else the slum. The hidden option 7
  (file 0xAB366) is an older version of the same, never offered.
- **The thieves** (state 0x24, $CITYT00, 1838:0000 of overlay 0x35 at
  file 0xAC140; also the map's random encounters, file 0x5E456, where
  DS:A891 is 3): an hour; the member of the best Perception
  (0E76:16FE(3)) is $ChosenOneName; unless an event of category 0x4F
  runs (0E76:32CE), if random(100) is over his Perception the thieves
  strike first: in a city the fight at once (on the map card 0 first).
  Else card 1 (card 2 on the map, without saints and alchemy):
  - *offer all your possessions* (file 0xAC6BE): the search
    (09C0:1EB9(−2) = 18E7:0854), card 3, a club each (09C0:2067(−2,
    15) = 18E7:0128);
  - *street sense* (file 0xAC722): if random(100) is at most the
    leader's Intelligence or Charisma, the higher, + (his skill 18,
    Woodwise, + Speak Common) / 2 (file 0xAC822; the game reads
    Streetwise on the map and Woodwise in a city, the reverse of its
    lessons), lessons of mode 1 in Streetwise and Speak Common for the
    leader, card 4, an hour; else card 5 and the fight;
  - *armed and dangerous* (file 0xAC88E): $NamedOneName the leader's
    weapon (09C0:1E9B, item 7 without one); if random(100) is at most
    his best weapon skill (0E76:01C0(−1)), a lesson in it, card 6; else
    card 7 and the fight;
  - *a saint* (0E76:2180; saints DS:EE4B..: Apollinarius, Genevieve,
    Godfrey): answered, card 8;
  - *run* (file 0xACA66): with horses (0E76:1326(5)) card 10; else an
    hour, and if random(100) is at most the slowest member's speed + 5
    (0E76:0656) card 11, else card 12 and the fight;
  - *alchemy* (file 0xACBE8): card 13, an hour;
  - *attack*: the fight.
  The fight (file 0xAC416): enemy 7 (the bandits) at variant s / 4 + 1,
  clamp(party size, 8, random(s)) of them, s = 09C0:1C1B (see the
  guardroom); the battlefield by the street the party came from
  (DS:A88D). Won: the reputation +1..+5 (0E76:19D0), card 15 (on the
  map), else 15, 17 or 18 at random; fled: 0E76:251A, card 11; lost:
  the reputation −1..−2, the search, a club each, card 16, two hours.
  Then back to DS:E7D8 (the map: state 0xC).
- **The shell game** (state 0xB2, $SHELL00, segment 18EA of overlay 0x58
  at file 0x110C20): the square (file 0x9D6EC) and the market (0x9F676)
  by day show it instead of their card when the city's property 0x23
  (0E76:1A8E: DARKLAND.CTY +0x6C, a day of the year, within 14 days of
  the first of the current month) holds and mark 0x2F is not set; it is
  then set for 24 hours. Paying (more than 12 pfennigs in the purse,
  file 0x110D02) takes a groschen (1367:0180) and shows card 4, 5 or 6
  at random; the right-hand shell wins with random(3) = 1, the others
  with random(4) = 1, three groschen (1367:0130) and card 7, but only
  once (DS:8E1C, never reset); else card 1..3 (the pea elsewhere, by
  random(100) % 2). Walking away returns at once to the square or the
  market (DS:A88D).
- Reproduced in `CityVisit`: all of the above in the city (the grove
  too), except the
  alchemy, the thieves' battlefield, s (0), 0E76:251A and the alchemy
  accident. The saint left unanswered is taken as card 9 and the fight
  (*inferred*, as the guards'). **verified** (code)

## News and rumors

State 0x66 ($CITYN00, 195D:0000 of overlay 0x45, segment 195D at file
0xE3A70; overlay 0x45 has 1838 at 0xE2820): reached from the inn (two
hours), the square (file 0x9DB66, an hour; a wanted check before, on
DS:584A, as the streets': not reproduced) and the slum (0xAB15A, city
size / 2 hours); "have learned what you can" (0xE3EF6) goes back
(DS:E7D8). Its options: the notices (state 0x6D), elsewhere in the
Empire (0x6E), the situation here (0xAE), special jobs (0x67), politics
(0xAE, offered with an event of kind 2 here, 0E76:3470). Almost all
their content comes from the game's events. **verified** (code)

- **Events** (0E76:360C(kind, a, location), file 0x5AEBC): a search of
  up to 300 events (far pointers at 7E30:1B2C) for one with +0x28 =
  kind, +0 = a, +0x1C = the location, still valid (0E76:3230). 0E76:
  3470(kind, location): one of category 8, started; 32FE(kind): one of
  category 8, started; 3C28(category, subject, +0x1A, kind, +0x2A),
  −1 for "any", returns its location (without looking at the dates).
  The records: formats.md "Events".
- **Card variables of places** (the switch at file 0x8E170): $Direction
  is one of "North", "Northeast"... (290E:20E3, 8 far pointers) by
  1462:29FA(x1, y1, x2, y2) (overlay 0x1E, segment 1462 at file
  0x5DAA0): dx = |x2 − x1|, dy = |y2 − y1| / 3; West/East when dx / 2
  >= dy, else North/South when dy / 2 >= dx, else the diagonal; from
  the location in DS:ED54 to the one in DS:E89B. 1462:271A(x, y): the
  nearest of the first 92 locations (the cities; not one at (x, y)
  itself), by max + min / 2 of dx, dy (dy a third). **verified** (code)
- **The notices** (state 0x6D, $OFFIC00, segment 1910 at file 0xE97C0,
  overlay 0x47): nobody reading better than 10 (0E76:14A4(12)): by day
  card 4 (a citizen reads them), at night card 5 and nothing more; then
  card 1 or 2 by the location's byte +0x14 (1, 2), card 3 with an event
  of kind 2 here, and card 6 when the city's property 0x21 is even,
  else card 0 (the curfew); no time; back to the news (DS:E896). Cards
  7..16 (executions, the Hussites, rewards, mines) come from events.
- **Elsewhere** (state 0x6E, $AFFAI00, file 0xE9FE4): no menu (card 0
  and its options are not shown by this code): card 5 with an event of
  kind 4, 17 with one of kind 0xC; then for the city: card 1 while its
  location's byte +0x14 has bit 0x80, card 2 while mark 0x42, card 4
  with an event of kind 2 elsewhere, else card 3 ("nobody has any
  travellers' tales"); back to the news.
- **The situation here** (state 0xAE, $SITUA01, file 0x10F96E): cards
  1, 2 by the location's byte +0x14 (bits 1, 2); then card 9 (bit
  0x80), 8 (mark 0x42), 3 (an event of kind 2 here), else by property
  0x21 % 20 (signed): 20 card 4 (never), over 14 card 5, over 9 card 6,
  over 4 card 7, else card 0; an hour; back to the news. Cards 10..20
  come from events.
- **Special jobs** (state 0x67, $SPECI00, file 0xE3F7A): option 0 while
  property 0x21 % 20 is 0 and not mark 0x65, 1 and 2 with an event of
  kind 2 here (0E76:360C(2, 0, location)), 3..8 hidden; the employers'
  names are 1367:0DB4(number + 12, ...) and (number + 22, ...).
- Reproduced in `CityVisit` with the game's events and the locations'
  state (a saved game's; a new game's are those of SAVES/DEFAULT): the
  menu, the notices, "elsewhere", the situation, one card after the
  other, the special jobs' menu (the jobs are quests: not
  implemented). Ended events count as gone (*inferred*: the game takes
  them away as time passes); $Direction is from the city where the
  party is (*inferred*: DS:ED54 is not set by these handlers);
  $NearestCity is the city nearest to the event.

## Time and travel

- The day: 1367:072A is true from hour 5 to 18, and the cities choose
  their day or night cards with it (state n by day, n + 1 at night,
  see "Game states"): night is the hours 19..4, whatever the bells
  say. `GameTime::IsNight()` follows it (it began at 21h, Compline,
  which showed the day cards two hours too long). **verified** (code)
- The date: DS:00E0 hour, 00E2 day (1-based), 00E4 month (0-based),
  00E6 year; the month lengths are the words at DS:2776 (31, 28, 31...,
  no leap years). 1367:05C8 `AddHours(n)` (file 0x5CD88) adds hours with
  the carries, after calling the functions that make time pass (0E76:
  255A, and 09C0:1F95 for more than 2 hours); 1367:0680 adds days. They
  are called about 2200 times, mostly by the card flows.
- **The passing of time**, 0E76:255A(hours), called by every
  `AddHours`, once: for each party member (status byte 1; 3 is reset
  to 0 and skipped), endurance is set to its maximum − 4 if more is
  missing, else raised by 1 if 3 or 4 are missing. If the hour plus the
  hours reaches 24: divine favor + clamp(1, 5, Religion / (random(5) +
  20)); strength + 1 if the party's best Healing (0E76:14A4(13)) is at
  least random(150); agility, perception and charisma + 1 each if
  random(100) <= 10. Then divine favor, strength, perception, agility
  and charisma are cut to their maximum plus the bonus of an active
  potion (0E76:4E0C: events of kind 0x48; none here). **verified**
  (code); see `PassTime()` in `Character.cpp`. (A day's gains come once
  per call, whatever its length.)
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

- **Places** (the map's state, 0xC, 1462:0000 of overlay 0x1E at file
  0x5DAA0; its loop at file 0x5DD60, a step at 0x5E6E4): a step onto
  a place of DARKLAND.LOC returns its index, and the next state is
  its location record's word +0x0C (file 0x5E6C3; the cities' is 4,
  before the walls). Else, after every step, encounters: DS:E488 (at
  most 10) grows by one when random(500) <= 9; there is one if
  random(1000) < (DS:E488 + 1) · 1462:0344(place, terrain) (file
  0x5F0E7; the place is the nearest, DS:907E); then with a place whose
  word +0x0E is not 0x62, six times in ten (random(100) <= 60) its
  territory: the state +0x0E (the castles' 0x92, the robber knight's
  land, $RAUBI02); else a random encounter (file 0x5E1DB: the dragon's
  and other events', then by the terrain, DS:D843). **verified**
  (code); the encounters are not reproduced.

## Camping in the wilderness

The bandits and the soldiers who find a camp ($CampB00, $CampJ00) come
from the camp screen on the map, not from a city (for the grove, see
"Encounters in the city"). **verified** (code); not
reproduced.

- **The danger** of a camp: the map (file 0x5E9C5) calls 0x9C0:1FF9
  with a base computed at file 0x6005E: 3 · [DS:A67E],
  plus a term from the nearest map cell of kind 0x1D within 5 cells
  (its distance − 6), plus the party's terrain (kinds 4, 5, 0x0F, 0x13:
  −1; 0x10, 0x14: −2; 0x11, 0x15: −3; 0x16: −5; 0x17: −6; 0x18: +1; 8,
  9: +2). *inferred*: what DS:A67E and kind 0x1D are.
- The camp starts (file 0x6F090) with the base in DS:8A36 and the
  danger DS:8A34 at 0 (or 0E76:39F0(0x61) while an event of category
  0x61 runs; 0 while one of category 0x60 does).
- **Each day** (file 0x70C10): with an event of category 0x60 running,
  the danger is 0. Else d = the base, less, for each member present
  (status 1) guarding the camp (activity 5): 3, 2 or 1 by Woodwise
  (70, 30), 2 or 1 by Stealth (70, 30), and 1 if Edged, Flail, Polearm,
  Thrown, Bow or Missile device is 30 or more (not Impact); d clamped to
  1..15 is added to DS:8A34. (The 128-byte records at DS:9C55: skill n
  is at +0x6B + n, 0E76:01A0.)
- **Before each day** (the S key, file 0x6FDFC), in the wilderness (type
  1): if random(50) + random(50) <= DS:8A34 the camp ends: DS:E7D8 is
  the guard (the lowest slot present with activity 5), else −1, mark
  0x5F for 2 hours (0E76:2930), and 0x11B.
- **The encounter**: back on the map, while mark 0x5F runs the step
  returns 1000 (file 0x5E9D2), and the encounter (file 0x5E1E5) is
  state 0x169 ($CampJ00, the soldiers) if random(2) is 0, else 0x16A
  ($CampB00, the bandits, file 0x17B89C). From the texts (*inferred*):
  card 0 when a guard saw them ($ChosenOneName; ignore, leave, a saint,
  ambush), card 1 when no one guarded (the fight at once); then the
  saint answered (2) or not (6), the ambush laid (3) or botched (4),
  the fight won (7), the escape (8, 10) or the defeat (9). The states' own code (overlay
  at file 0x17AF90..) is not decoded.

## The church

The city church ($CITYC00.MSG by day), segment 1838 (file base
0xB8840): the card at 1838:0000, the options dispatched by a switch at
file 0xB89E0. **verified** (code); see `CityVisit.cpp`.

- **The card**: $Money1 is a tenth of the party's purse, in pfennigs;
  "give $Money1" is disabled when that is under 10. "Seek sanctuary" is
  disabled when the local reputation is over −10 and the party is not
  wanted (0x9C0:20F3: see "Sanctuary").
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
  (−1, 9, 1, 10, −1) is called: 1462:0132, a chance of a point of
  Virtue (skill 9, see "The residence"). The leader gains
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
- **At night** ($CITYC01.MSG, file 0xB9249, state 0x35; the same segment
  base 0xB9230 for its tables): Mass at Matins in cities of size 7 or
  more, at Latins always, at Prime 5, Nones 4, Vespers 6, never at
  Terce, Sexts and Compline (file 0xB93CF); it gives Religion / 60 +
  Speak Latin / 40 + 1 divine favor, the same wait, card 2; otherwise
  card 1 with the next Mass (Prime, or Vespers at Prime in towns of size
  4). The altar boy (file 0xB9572) names the next Mass by bell: Matins
  (at Matins, size 7 or more), Terce (at Prime, size 5 or more), Vespers
  (at Prime in size 4, at Nones in size 4 or more), Compline (at
  Vespers, size 6 or more), else Prime; at Terce, Sexts and Compline it
  reads an unset variable (the night card is not shown then). Sanctuary
  shows card 4 and goes to state 0x81 (see "Sanctuary"); leaving goes to
  the churches (0x13).

## The monastery

The monastery by day is state 0x36 ($CITYM00, 192F:0002 of overlay 0x39,
segment 1838 at file 0xB8840: file 0xB97B2), at night state 0x38
($CITYM01, overlay 0x3A, file 0xBBAAC). The decks $MONAS00 and $MONAS01
are never loaded (no "$Monas" string in the executable). **verified**
(code)

- **Arriving by day** (file 0xB9825): with a reputation of −40 or less,
  or the best Virtue (0E76:14A4(9)) + the fame / 20 at most 15, card 1
  and the churches (state 0x13); else card 5 while mark 0x32 runs, or
  card 0 and mark 0x32 for 12 hours. $Money1 is 5 groschen a member
  (DS:A67E · 60 pfennigs). Offered: prayers (not while mark 0x30, nor
  with a purse short of $Money1), tutoring (not while marks 0x28, 0x33),
  the library (not while mark 0x34), healing (not while mark 0x31),
  sanctuary, leaving; the monks' problems, the abbot and "a quiet
  place to pray" are not.
  - *prayers* (file 0xB9B3A): card 2, random(2) + 1 hours, the price,
    each member's divine favor + Religion / 9 + 1 (0E76:0A72), card 3,
    mark 0x30 for 168 hours;
  - *tutoring* (file 0xB9C86): card 4, two hours; the chance (file
    0xB9E22) is the average Virtue + the reputation + the fame / 50, or
    the best Virtue + the best Charisma if higher (1367:0084, a max;
    then the most charismatic is $ChosenOneName), within 1..99: three
    teachers for 168 hours (0E76:2C4E, category 0x28, level 50 at
    +0x20, the fee at +0x1E: Religion 50 pfennigs a day, Speak Latin and
    Read & Write 25) and card 8, whose $Money1 is the city size + the
    purse / 150, within 12..180, / 12 groschen (not what they charge);
    else mark 0x33 for 55 hours and card 6;
  - *the library* (file 0xB9EA6): the chance (file 0xBA00C) is
    0E76:179C(11, 12), which walks the members for the highest maximum
    Intelligence + Charisma against twice the Intelligence kept so far
    and gives that (the fields 11, 12 of its table: Latin and reading
    seem meant), + the reputation / 2 when negative, + 25 after
    prayers (mark 0x30), within 0..99, 100 from 75 up. Success: card 4,
    an hour, mark 0x34 for 1440 hours, state 0x39 ($LEARN00?) and then
    the churches; failure: an hour, mark 0x34 for 720 hours, card 4 and
    card 7 ("too busy": no prayers paid for) or card 16;
  - *healing* (file 0xBA074): card 4, an hour; the chance (file 0xBA1C6)
    is the average Virtue + 20 when a member's Strength is at most 90 %
    of its maximum, else 0; success: the abbess (state 0xB4); else card
    10 (nobody hurt enough) or 11, and the churches;
  - *sanctuary*: card 12, the churches; *the abbot* (hidden): with
    unrest here, card 9 and the politics (state 0x6F).
- **At night** (file 0xBBB13): card 1 ("come back in the morning") and
  the churches with a reputation of −40 or less, or, as the code has
  it, a best Virtue + fame / 20 of 20 or more. Offered: prayers (as by
  day), "we perish!" (not while mark 0x31), sanctuary, going elsewhere.
  - *prayers* (file 0xBBD8E): the chance is the reputation / 2 + the
    leader's Virtue / 4 + his Charisma / 2, within 0..99 (file
    0xBBED4): the price, card 2, divine favor + Religion / 10 + 1, mark
    0x30 for 168 hours; else card 3, and the reputation − 1 while mark
    0x35 runs, else mark 0x35 for 8 hours. Then the churches.
  - *"we perish!"* (file 0xBBFDA): card 4 (a monk calling the party
    scoundrels: not reproduced), an hour; the chance (file 0xBC0A6) is
    the average Virtue + 10 when a member's Strength is at most 80 % of
    its maximum, else 0: card 5 and the abbess; else mark 0x35 for 8
    hours, card 6, the churches.
  - *sanctuary* (file 0xBC12C): mark 0x35 for 8 hours, card 8, the
    churches; the monks' problems and the abbot (hidden): card 7.
- Reproduced in `CityVisit`: all of the above but the library's reading
  (state 0x39), the abbess (state 0xB4) and the abbot.

## Reputation

0E76:1B12 turns a reputation into the index of its word, in the list
"a local hero", "respected", "unknown", "suspected", "wanted", "hunted":
over 50 → 0, over 10 → 1, over −10 → 2, over −40 → 3, over −75 → 4,
else 5. **verified** (code).

## The menu bar

The hidden bar across the top of the screen (manual pp. 17-19): the right
mouse button held down, or F10, shows it; it has four pull-down menus,
**Game**, **Orders**, **Attack** and **Party**.

- **The data** (all **verified**, DARKLAND.EXE): the texts are at DS:0CA1
  (file 0x191A61) to DS:0DF0. The function at file 0x10A0E (`enter 0xE`)
  fills the records at startup: 5 menus at DS:9419, 10 bytes each (+0 the
  title's pointer, +6 a state byte: 1 for Game and Party, 2 for Orders
  and Attack, +8 the pointer to its items; the fifth, with an empty
  title, ends the list), and the items, 20 bytes each (+0 the name's
  pointer, +2 an enabled byte, +3 the shortcut's 3 bytes, +0xE the far
  pointer to its handler, *inferred* an RTLink thunk of segment 09C0); the
  Difficulty sub-menu (DS:99EC) has items of 22 bytes. The loop that sets
  the enabled byte to 1 covers the Game items (its 8 and the terminator),
  the Difficulty items and the Attack items; the Orders items start at 0
  (*inferred*: the battle code turns them on), and so is Party Info.
- **Texts and shortcuts**: a name starts with a glyph of font 0 (FONTS.FNT,
  the small one, height 7): 01 is a checkmark, 02 a blank of the same
  width; the game changes the first byte to check or uncheck an item
  (Show Changes, Music and Sound FX start with 01). The shortcuts are
  texts too, with the glyphs 0F "Alt", 0A "F6", 12 "Rtn", 13 "Spc", 03
  "Esc" (rendered, **verified**):
  - *Game*: Save Game (Alt S), Load Saved Game (Alt L), Difficulty (Alt D;
    its sub-menu Basic, Standard, Expert), Show Changes (Alt C), Music
    (Alt M), Sound FX (Alt F), Pause (Alt P), Quit to DOS (Alt Q).
  - *Orders*: Resume (Spc), "?" (Rtn; the game puts the selected
    character's name and "Finished" there, as the manual says), Enemy
    Info (E), Walk towards (W), Flee towards (F), Halt (H), Travel As Group
    (G), Travel Single File (Q), Use Door (U), Use Stairs (U), Open Chest
    (O), Pick Lock (P), Dissolve Lock (D), Disarm trap (D; the texts at
    DS:05B2..0643 are three kinds, simple, moderate, complex, and the same
    "(Ldr)" for the leader), Surrender (All) (S), Loot Bodies (L), Exit
    Battlefield (X), Cancel Giving Order (Esc). Some names end in blanks,
    which widen the menu.
  - *Attack*: Throw (T, with a sub-menu of potions), Std Attack (A),
    Vulnerable (V), Berserk (B), Parry (P), Use Missile (M).
  - *Party*: Party Info (F6), Change Marching Order (Alt O).
- **Not in this executable**: the manual and its screenshot also have
  "Visuals" (Full, Quick, None), "Set Ambush" (F7) and "Camp" (F8); the
  strings are nowhere in DARKLAND.EXE (searched in the whole file), so
  this version of the game has no such items: they are *not* in the bar
  here (the ambush and the camp of the map are not a menu item either).
- **How it behaves** (the manual): the right button down shows the bar,
  moving with it down opens the title's menu, releasing on an item selects
  it; with the keyboard F10, the cursor keys, Return, and F10 again to
  leave. Items not allowed are dim. A menu's title is dim when it has
  nothing to choose. The Save is "faded" where saving is prohibited
  (battles: *inferred*; the string "Battlefield save rules are in effect"
  is at file 0x192536).
- **Change Marching Order** (09C0:183D = 1EF8:051C of overlay 0x16, file
  0x51050; the function at file 0x5156C, **verified**): for each place
  but the last it asks "Select %Fs" (a string of a table loaded at run
  time, DS:7DF2:0267 + 4 · place: not found) and the player picks, by the
  mouse or F1..F5, one of the members left; the last takes the last
  place; the order is written at DS:9064.
- **Not decoded**: the drawing (the colors, the exact size of the bar and
  of the menus are measured on the screenshot of manual p. 17, *inferred*)
  and what most handlers do.
- **Implemented** (`MenuBar`, `GameSettings`): the right button (the world
  map's city details are on the middle button now), F10 and the Alt
  shortcuts in the cards, the map, the trade and the residence screens
  and the battle. The Game menu works: Save Game (as Ctrl+S), Load Saved
  Game (a card lists the saved games, the newest first, ten at most:
  *inferred*), Difficulty (written to the saved game, byte 0x96; the
  fame of a task uses it, 1462:1E06), Show Changes, Music and Sound FX (only
  kept: the game has neither the messages nor the sound), Pause (waits
  for a key), Quit. Change Marching Order asks who goes first, second... (the prompts are
  made up; not in a battle or at a merchant); Party Info opens the F6
  screen. In a battle Resume
  and Halt work (and H); the other orders of the menu are dim, as the
  battle has no such orders yet.

## Battles

Where the battle code starts; the rules are not decoded yet.

- **Setup** (the overlay at file 0x16E40, the function at its offset 0):
  it opens darkland.enm, darkland.lst and imaps.cat (DS:13A2, pushed at
  file 0x16EEA), then switches on its first argument, 0..0x89, through
  the jump table at file 0x16F34: the battlefield type, TAC.TXT's
  `bfldtype` (**verified** with the night watch's battle, below).
  Each case calls, with the same arguments,
  the entry (offset 0 unless noted) of segment 1432 in one of seven
  overlays; that function builds the battlefield. **verified** (code):

  | bfldtype | overlay, file | what it builds |
  |---|---|---|
  | 0..8, 39, 40, 122..126 | 5, 0x1CA70 | not identified (no map name) |
  | 10..16, 102..105, 108, 109, 127..137 | 9, 0x2BCD0 | mines: "iminegen" / "iminspec" |
  | 17..38, 41..45, 86, 106, 107 | 0xA, 0x32480 | cities: "icity..." |
  | 46..85, 97..99, 111..121 | 8, 0x289D0 | the wilderness: "iwild..." |
  | 9 | 8, 1432:2D62 (0x2B732) | the wilderness, another entry |
  | 87..93, 110 | 0xD, 0x38090 | the castle of LCASTLE (see below) |
  | 94 | 0xB, 0x35360 | fortresses and monasteries: "ifortmon" |
  | 95 | 0xB, 1432:1FB2 (0x37312) | tombs: "imisctom" |
  | 96, 100, 101 | 0xC, 0x37530 | "cavedrag", the dragon's cave |

- **Calls through RTLink**: the cases call 09C0:1B49.. in the root. Each
  is a 10-byte entry of RTLink's table (file 0xCBE3..0xE94D, 753
  entries): `E8 rel16` (a call to the overlay manager), `EA off seg`
  (the target) and a word, the overlay's number. Segment 09C0 in the file
  is 0x1800 + 0x9C00 as usual. Several overlays share a segment number
  (here 1432): the pair (segment, overlay) names the code. An overlay
  starts with its relocations, 4-byte `off seg` entries (e.g. `xx xx
  32 14` for 1432), then its segments' code, in the order of their
  numbers: the code of 1432 in overlay 5 is at 0x1CA70 and that of 146D
  at 0x1CE20 = 0x1CA70 + (0x146D - 0x1432) · 16. **verified** (the
  relocations end at the found bases; no target reads more arguments
  than its case pushes, 10 or 11 words)
- **From the map to the battle**: the builders load the map (13308 =
  0x33FC bytes, e.g. the wilderness at file 0x289D0 into a buffer at
  [DS:DF7F]:[DS:DF81], through 05D9:019A, which loads a catalog entry at
  segment:offset), change it, and write it to LEVEL0.FLR (1262:0D0A).
  The tactical code reads and writes the level%d.flr files at segment
  20A5, offset 0 (file 0x141EE and nearby), and addresses a cell as
  20A5:(y · 40 + x) · 4 (e.g. file 0x5222A). **verified** (code)
- **Drawing**: no battle file holds the ground or the walls, and no
  code near the map (segment 20A5) computes screen addresses: the
  screen is drawn through **MGRAPHIC.EXE**, MicroProse's MCGA driver
  (7208 bytes, "09-19-91", the name pushed at file 0xEF03), whose table
  of function offsets starts at its file offset 0x22A (the same kind
  of driver as Civilization's MGRAPHIC.EXE). DARKLAND.EXE references
  segment A000 only 7 times. How the battle draws is still to be found:
  through the calls to the driver's functions.
- **The graphics layer** is root segment 047C. 047C:07CE loads the
  driver (it is passed "mgraphic.exe" and "fonts.fnt", file 0x10700).
  047C:04C7..061E are the entries the game calls (e.g. 047C:055A 341
  times, 047C:1293 and 047C:12C3 over 500 times each, from the whole
  program): in the file most are `jmp far 0000:0000` slots, filled in
  when the driver is loaded, and some call a common handler at 047C:03CA;
  their spacing is not regular (mostly 7 bytes). Which slot is which
  driver function is not decoded; the driver's own table of offsets is
  at MGRAPHIC.EXE file 0x22A. The battle code (files 0x3C000..0x54000)
  calls 047C:0553, 055A, 05A5, 073D, 0744, 1120 and others.
- **Combatants**: DARKLAND.EXE keeps a debug panel of the combat
  variables (file 0x4A4C6): it prints the names at DS:1C68.. (a table of
  pointers at DS:1F2C) and, for combatant i, the values of the 128-byte
  record at DS:9C55 + 128 · i (the party slots come first: the status
  byte DS:9C69 of "Runtime and globals" is its field +0x14) and of
  arrays indexed by i. In the order printed, **verified** (code):

  | name | where |
  |---|---|
  | ManX, ManY, xpos, ypos, destx, desty | record +0, +2, +4, +6, +8, +0xA (words) |
  | Orders | record +0x10 (word) |
  | Status | record +0x14 |
  | SeqNum, TrueDirn, Facing, Seq | record +0x18, +0x19, +0x1A, +0x1B |
  | Target | record +0x4A |
  | StrikeSpd | record +0x1F |
  | MissileWpn | record +0x22 |
  | HitResults | DS:CA57 + i |
  | HitChance | DS:D493 + i |
  | CombatSeqIP | DS:CA64 + i |
  | HitBy | DS:CAA7 + i |
  | OrigFacing | DS:D3FE + i |
  | Automove | DS:A6D3 + i |
  | WeaponHighStr, WeaponMinStr | record +0x57, +0x56 |
  | CloseUpMove | DS:D41B + i |
  | PCMeleeAttack, PCMissileAttack | record +0x20, +0x21 |

  A second page (file 0x4AA08, names from DS:1F60) prints the rest,
  **verified** (code): Endurance +0x5D, Max Endur +0x64, Strength +0x5E,
  Max Str +0x65, Agility +0x5F, Perception +0x60, Intell +0x61,
  Charisma +0x62, Favor +0x63, PCLevel DS:A6B4 + i, MeleeWeapon +0x51,
  WeaponSpd +0x52, WeaponHndPen +0x53, WeaponDmg +0x54, WeaponMinStr
  +0x56, WeaponQual +0x0D, WeaponSkill +0x59, MeleeWeapQual +0x58,
  ShieldQual +0x5B, ShieldType +0x5C, ArmorStr(0) +0x4D, ArmorStr(1)
  +0x4E, Armor(0) +0x4B (StrikeResult is computed; Armor(1) is past the
  part read). The record follows the character record (formats.md):
  the attributes at +0x5D and +0x64, the weapon at +0x51, the vitals
  armor at +0x4B, the shield at +0x5C are the same fields; the bytes
  +0x52..+0x5B, not decoded there, hold the weapon's figures.
- **HitChance** (file 0x43CA2, for combatant i, when it is 0xFF): with
  bit 1 of Orders (0x2) set, 6000 / d, where d = (200 − WeaponSkill −
  Agility) / 2 + (WeaponSpd + 15) · 2; then − 30 if Orders has both bits
  0x6, else + 60 with both bits 0x22, else + 120 with both bits 0xA; + 30
  if (Max Endur + Max Str) / 2 < [+0x49] ≤ Max Endur + Max Str, else
  + 100 if [+0x49] > (Max Endur + Max Str) · 3 / 2 ([+0x49]: the load
  carried? *inferred*). Without bit 0x2 it is 100, and field +0x1D gets
  150 − WeaponSkill / 2 − Agility / 2 (at least 0). **verified** (code).
  The bigger the number, the faster and more skilled the fighter: more
  a rate of strikes than a chance (*inferred*).
- **Weapon table**: 63 weapon types (the item type of a weapon: a
  character's weapon in use, +0x51), static data in segment 20A5 (file
  0x17F670, as the data segments: the far pointers at DS:7D8C..7D9E all
  hold 20A5), one array of 63 per figure: category at 75AC (0 edged, 1
  impact, 2 flail, 3 polearm, 4 thrown, 5 bow, 6 missile device: the
  order of the weapon skills), a 2-letter code at 75EB (2 bytes each),
  speed at 7669, hands and penetration at 76A8 (high nibble 1 or 2
  hands, low nibble the penetration), damage at 76E7, the skill it
  needs at 7726, minimum and maximum useful strength at 7765 and 77A4,
  range at 77E3 (missiles only); the armor strengths by armor type at
  781E. **verified**: the codes are those of the sprites' file names
  and agree with DARKLAND.LST's items (0 Two-hand Sword S2, 1 Long
  Sword SW, 6 Battle Axe A2, 27..29 bows BW, 30, 31 crossbows CB, 32,
  33 handguns HG); 35..62 have no item and are the monsters' natural
  weapons, whose codes are those of their sprites (48 SK the skeleton,
  M03CBSK; 44 WF the wolf; 46 GN, 47 KB, 37 GG, 39 VL...). E.g. the
  long sword: speed 55, one hand, penetration 3, damage 12, skill 18,
  strength 19..29.
- **Setting up a combatant** (file 0x446E0, combatant i, weapon type
  w): the record gets the weapon's speed (+0x52), hands and
  penetration (+0x53), damage (+0x54), needed skill (+0x23), strength
  range (+0x56, +0x57), range (+0x0C), the armor strengths of its
  vitals and limbs armor types (+0x4D, +0x4E), WeaponQual (+0x0D) = the
  melee weapon's quality (+0x58) or the missile weapon's (+0x5A), and
  WeaponSkill (+0x59) = the character's skill (+0x6B + category). With
  skill S, Max Str T, weak = min − T and unskilled = needed skill − S,
  each counting only if above 0, and strong = T − max if above 0:
  PCMeleeAttack (+0x20) = S + 2 · strong − 3 · weak − 2 · unskilled,
  PCMissileAttack (+0x21) = S − weak − 2 · unskilled, both clamped to
  0..255. **verified** (code)
- **A melee strike**, as DARKLAND.EXE resolves it. **verified** (code)
  unless noted; "record" is the combatant's (above), die(n) is
  0000:0C8C, 1..n (rand() % n + 1, n at least 2).
  1. *Whether it strikes now* (file 0x43F48): roll = die(100); no strike
     (result 0) if HitChance < roll. The same roll is used below.
  2. *Hit location*: the roll's parity: even the vitals (0), odd the
     limbs (1).
  3. *Chance to hit* (file 0x44232, attacker A, defender D): the
     defense is D's PCMeleeAttack (+0x20) plus, when A's weapon is not
     of category 2 (a table at [DS:7D8C]:75AC by weapon type) and D's
     WeaponHndPen (+0x53) has 1 in its high nibble (one-handed?), D's
     shield: ShieldQual · 2 / 7 for shield type 0x5F, · 2 / 5 for 0x60,
     / 2 for 0x61; otherwise −5; at least 0. chance = (A's PCMeleeAttack
     − defense) · 2 / 3 + 50 + m, at least 10 if A's Orders has bits
     0x6. m: −5 if A's Orders has bits 0xA, else −2 − A's WeaponSkill / 4
     with bits 0x22, else max(A's WeaponSkill / 4, 10) with bits 0x6;
     then if D's Orders has bits 0x22, m −= D's WeaponSkill / 4 + 5, else
     with bits 0x6, m = min(−(A's WeaponSkill / 4), −10); + 10 for every
     combatant fighting D (status 1, Orders bit 0x2, target D), − 10
     for every one fighting A; − 15 if A is in the list at DS:A2EB
     (0E76:469E; not decoded).
  4. *Result* (file 0x441EC): roll ≤ chance − 10: a hit (2); roll ≤
     chance: a weak hit (3); else, if roll ≤ 5 and A's WeaponSkill −
     A's [+0x23] ≥ 2 · roll, a hit (2); else a miss (1).
  5. *Damage* (file 0x44418, location L): armor = D's ArmorStr(L)
     (+0x4D + L) + (D's [+0x55] & 0x7F); penetration = (A's WeaponHndPen
     + A's [+0x48]) & 0x0F, + die(4) if A's Orders has bits 0xA, − die(4)
     for a weak hit; base = A's WeaponDmg (+0x54) + A's [+0x48] >> 4,
     whole if armor < penetration, half if equal, an eighth if more;
     A's Max Str above WeaponHighStr adds (Max Str − high) / 5 + 1,
     below WeaponMinStr adds (Max Str − min) / 5 − 1; then + (A's
     WeaponQual (+0x0D) − q) / 10, q = D's armor quality (+0x4F + L) +
     A's [+0x24] / 2 if A's [+0x55] has bit 7, at most 99.
  6. *Rolling it* (file 0x4456A, s = that value clamped to 0..40, r =
     die(6) + die(6)): s ≤ 7: r / a[s] + b[s] with a = 13, 7, 5, 7, 5, 4,
     3, 3 (DS:1B90) and b = 1, 1, 1, 2, 2, 2, 2, 3 (DS:1B88); s ≤ 20:
     r / 2 + s − 6; else s + r − 10. It is added to D's Endurance loss
     (DS:D70C + 2 · D); D's Strength loss (DS:D70D + 2 · D) gets that
     times (3 + die(6) if armor < penetration, 1 + die(4) if equal, else
     random(3), 0..2, 0410:0008) / 10.
  7. *Applying it* (file 0x4462E): the Endurance loss is at most 42 per
     strike (the damage numbers of BATTLEGR.IMG go to −42); Endurance
     (+0x5D) at 0 sets the status to 2, Strength (+0x5E) at 0 to 3
     (*inferred*: unconscious, dead).
- **After a battle** (0E76:2278, the function every battle goes
  through: it runs the battle with 09C0:18BF, then from file 0x59B9C):
  a party slot with status 3 (Strength at 0) is emptied (status 0,
  09C0:18B5: the member leaves the party); every other member (but
  status 5) stands again (status 1), keeping the Endurance and Strength
  of the battle; with nobody left, 09C0:18F1(0x12) and the game ends
  (not decoded further). It returns the battle's result ([DS:A893]).
  **verified** (code)
- **Loot**: the battle's orders menu has "Loot Bodies" (DS:718, with
  "Open Chest", "Pick Lock", "Dissolve Lock", "Surrender (All)", "Exit
  Battlefield"). The loot screen is an overlay of its own (segment 1462,
  its relocations from file 0x8A320, code from 0x8A4E0): 1462:0000
  takes the pile (a far pointer to the items and their count) and a
  far pointer to the cash (florins, groschen, pfennigs); it treats
  arrows, quarrels, balls (types 0x40..0x42) and thrown weapons
  (0x17..0x1A) apart; 1462:01C8 is the exchange screen, like the inn's
  cache: "Get an item from pile of loot", "Put an item into the pile of
  loot", "Distribute to a different person", "Leave" (DS:4C42..4CAE),
  "The loot contains...", "%s currently has...", and "The party finds
  %dfl, %dgr, %dpf in cash." **verified** (code). What the pile holds
  (the fallen foes' items? TAC.TXT's F, G, Pf of the activation spots
  for the cash) comes from the battle code that calls it, not found:
  no RTLink entry leads to 1462:0000. Reproduced (TradeView::SetLoot())
  with a provisional pile: the fallen enemies' weapon, armor and shield
  (real items only), at their type's qualities, and no cash.
- **LCASTLE** (in the game directory) is a catalog like the .CAT files:
  LC_COURT, LC_G1..LC_G5, LC_G4S, LC_G4NS, LC_G7A, LC_G7B and an .ATV
  for each, the castle's levels (overlay 0xD names "lcastle",
  "lc_court.fil", "lc_court.atv"...). Not decoded.
- The debug dump of TAC.TXT: "Tac params are..." (DS:1005) is pushed at
  file 0x12443, "Enemy Activation Record for level %d" (DS:2406) at
  0x535F2; battlegr.img (DS:121C) is loaded at 0x12BA2.
- **Map names** (DGROUP strings, pushed to sprintf):
  "iwild%s.%03d" (DS:1486, file 0x28E78), with the kinds "gen", "mtn",
  "msh", "spc", "sab", "gat", "wal" (DS:146A..1482);
  "iminspec.%03d" (0x2CC25), "iminegen.%03d" (0x2CCC3);
  "icity.%03d" (0x32D7C), "icity.70%d" (0x339A1), "icity.80%d"
  (0x34016), "icity.600" (0x349B4); "ifortmon.%03d" (0x357FF);
  "imisctom.%03d" (0x37330). **verified** (code)
- **The wilderness** (the function at file 0x28C46): the name is
  "iwild" + kind [DS:DF85] + the number [DF87] · 100 + [DF89] · 10 +
  [DF8B], as the catalog's IWILDGEN.101..316 (hundreds 1..3, tens 0..1,
  units 1..6). The three are set by nested switches (on [DF83], and on
  DS:00E4, the month, 0..11, in the function at 0x28E9E), partly at
  random (0E76:20F6, `random_range(low, high)` *inferred*). Not decoded
  further.
- Other names near them: "terrain.fil", "level%d.trn", "level%d.atv",
  "level%d.flr", "keep", "keep1", "citynw1%1d", "cityww1%1d": the files
  the battle writes while it runs (*inferred*: TERRAIN.FIL is in the
  game directory).
