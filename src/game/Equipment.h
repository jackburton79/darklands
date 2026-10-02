/*
 * Equipment.h
 * What a character carries and what he has in use: the rules of
 * DARKLAND.EXE's item functions (overlay segment 18E7, file base
 * 0x65C30; docs/exe.md, "Equipment"). An item in use is the carried item
 * of the slot's type and quality (18E7:14B6).
 */
#pragma once

#include "Character.h"
#include "ListFile.h"

#include <vector>

static const size_t kMaxCarriedItems	= 64;
static const int kMaxItemQuantity		= 255;
static const int kMaxWeightInUse		= 500;

// The slot an item goes to when it is readied, or -1 for the items
// that cannot be used (18E7:11B2): edged, impact, polearm and flail
// weapons are weapons; thrown, bow and missile devices are missile
// weapons; the armors and shields by item type
int SlotForItem(const item_definition& definition, uint8 type);

// The carried item in use in a slot, or NULL
const item* ItemInUse(const character& member, int slot);
// The slot an item is in use in, or -1
int SlotInUse(const character& member, const item& carried);

// Makes the item at `index` the one in use in its slot; false if it
// cannot be used (18E7:11B2)
bool ReadyItem(character& member, size_t index,
    const std::vector<item_definition>& definitions);
// Takes the item at `index` out of use (18E7:12E8)
bool UnreadyItem(character& member, size_t index);
// The weight of what is in use, at most 500 (18E7:140A); also kept in
// the record's byte 0x49
int WeightInUse(character& member);
// How loaded a member is (0E76:013A): 0 if the weight in use (a signed
// byte, as the game keeps it) is at most Endurance + Strength, 2 up to one
// and a half times that, else 3
int EncumbranceClass(character& member);
// A member's speed (0E76:0656's measure, 0E76:7F8E): the Agility, two
// thirds of it when loaded (2), 1 when overloaded (3)
int MemberSpeed(character& member);

// Adds an item: to the stack of the same code, type and quality if
// the quantity stays under 256, else as a new one, if there is room
// (18E7:01B4)
bool AddItem(character& member, const item& added);
// One of the item, or all of them: when none is left it is taken out of
// use and out of the list (18E7:0474 and 07D4)
bool DropItem(character& member, size_t index, bool all);
// Gives one of the item or all of them to another character, whose
// list must not be full (18E7:0000 and 007C)
bool GiveItem(character& from, size_t index, character& to, bool all);
// Moves the item at `from` to the place `to` of the list (18E7:10D2)
bool MoveItem(character& member, size_t from, size_t to);
