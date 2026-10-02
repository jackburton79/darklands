# Extras: what DARKLAND.EXE does not have

The program follows the original game. Anything that the original does not
have is an **extra**: it is off by default and is switched on and off in
game, by the one global setting, the **Extras** item of the Game menu (Alt
X; `game_settings::extras`, `MENU_EXTRAS`). With it off the game is as
DARKLAND.EXE has it. The item itself is not in the original's menu either.
The setting is not saved (like the other items of the menu, except the
difficulty).

Rules for a new extra: it reads `extras` when it draws or acts (not once at
the start, so that the menu changes it at once); with it off nothing
changes, not even a pixel; it is listed here, with what the original does.

## The trade scrolls (`TradeView`)

The original (BUYSELL.PIC, the exchange scrolls, docs/exe.md "Trade") gives
no sign that a scroll holds more rows than the four it shows, and no sign
of the items in use: the rows are `%5upf  %Fs  (%3d) %2dq`, and selling an
item in use takes it out of use at once.

With Extras on:

- **Arrows on the rods.** An arrow pointing up on the rod above a scroll
  when rows are hidden above, one pointing down on the rod below when they
  are hidden below (the rods are the places to click to scroll).
- **A mark for the items in use.** A small crimson diamond at the start of
  the row, in the lower scroll (the member's items), for the item in use in
  a slot (`SlotInUse()`). The rows of that scroll start `kMarkWidth` pixels
  to the right, marked or not, to stay aligned.

## The cards (`CardView`)

The original (0265:0134, docs/exe.md "Disabled options") lays out a
disabled option, the option word 2, like the others: same color, but no
area, so it never lights and cannot be chosen. Nothing shows that it is
off (*inferred*: the layout passes the same color to every line).

With Extras on, the lines of a disabled option are drawn in EGA dark gray
(`kDimColor`) instead of the text color.

Not done yet (ideas): the same in the Equipment scroll of the information
screens; a warning when an item in use is sold.
