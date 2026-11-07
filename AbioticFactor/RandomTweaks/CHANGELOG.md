# RandomTweaks changelog

## 1.5.0
- The teleporter tag menu is only as tall as its name list: it shrinks with the number of names in use and stops growing at eight rows.
- The tag menu shows its scrollbar only when the list is longer than the rows it fits.

## 1.4.0
- Instant butcher: corpses pop on the first hit from a weapon that can gib; living enemies and the killing blow are never gibbed.
- Built against UE4SS 3.0.1 (d4aebbf0).

## 1.3.0
- Nameable teleporter pads: type any name into the pad's tag box; pads with the same name link (case-insensitive, up to 32 characters). Config key: nameableTeleporters.
- The pad's tag menu lists the names in use, with the number of pads per name, instead of the premade tags.
- Named pads show their name in the interaction text, like tagged pads do.

## 1.2.0
- Updated for Abiotic Factor 1.5.0 (Modding Update), built against UE4SS 3.0.1.
- Fixed a memory overrun in the Wishing Shelf top-up caused by the 1.5.0 world flag change.
- The Wishing Shelf top-up now disables itself, with an error in the UE4SS log, if a game update changes the inventory layout.
- Reduced lookup overhead and memory growth in the Wishing Shelf top-up.

## 1.1.0
- Allow hiding full suit & backpack via transmog.

## 1.0.1
- Small fixes

## 1.0.0
- Initial release
