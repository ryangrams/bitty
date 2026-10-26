# Skin sources and original themes

Research checked October 26, 2026. This is a curated list of ten author-hosted art packs. A defensible global ranking of the ten most-used Flappy Bird skins was not available. [itch.io's Flappy Bird asset listing](https://itch.io/game-assets/tag-flappy-bird) helped find candidates; its storefront ordering measures a different thing from installed use.

The ten palette presets in this firmware, including Original, are color treatments of the locally supplied base artwork. The two project-created themes, Toast on the Run and Past Due, use newly drawn graphics. The ten third-party packs below are optional sources for further user imports. These are three separate sets.

The machine-readable [catalog](../assets/skin_catalog.json) contains source links and rights notes only. No listed artwork or audio is bundled with it. The license summaries record each author's stated terms and do not certify ownership or clear a finished game's presentation.

## Ten curated packs

| Pack and source | Theme and supplied material | Stated rights and import considerations |
| --- | --- | --- |
| [Tappy Plane, Kenney](https://kenney.nl/assets/tappy-plane) | Plane flight pack with 75 files. | CC0. Download archive layout was not inspected. Resize and map the chosen aircraft, obstacles, scenery, and UI locally. |
| [Flappy Bird Assets, MegaCrash](https://megacrash.itch.io/flappy-bird-assets) | Two player styles, 14 player colors, ten backgrounds, and 40 pipe/tileset variants. Pixel assets use a 16-pixel grid. | Author marks the pack CC0. A ZIP pixel pack and separate vector pack are offered. Check the artwork and rights for your intended use. |
| [Little Bird, Foozle](https://foozlecc.itch.io/little-bird) | A small bird with a large sword. Eleven animations, sprite sheets, and Aseprite source. | CC0, including modification and commercial use. Choose three readable flight frames and supply the remaining environment/UI locally. |
| [Pixel Art 2D: Flappy Bird-Like, DemonStick Games](https://demonstick-games.itch.io/pixel-art-2d-flappy-bird-like) | Bat, cavern scenery, terrain, obstacles, sprite sheets, and sounds in a ZIP. | Free use with the credit PoorGameDev. The page does not publish a standard license identifier. Preserve that credit and verify redistribution scope. |
| [Tappy Sub, MathMage / Mei Bunny](https://mathmage.itch.io/tappy-sub-assets) | Submarine, propulsion bubbles, cave layers, rock pillars, and UI in `Assets.zip`. | Author permits use and modification with credit. A 38 by 38 submarine and 320 by 240 background need adaptation to the portrait canvas. |
| [Flappy AirShip, Mike](https://pixelbitsnbytes.itch.io/flappy-airship-gfx-asset-pack) | Animated airship, sunset scenery, columns, font, and source model in a ZIP. | Author permits game/commercial use and bars reselling the asset package. Designed for at least 1024 by 768, so substantial simplification is needed for this screen. |
| [Cute Flappy, Gufiao Studios / Jovanny Game Dev](https://jovannyrodz.itch.io/cute-flappy-assets) | Free red bird with six PNG animation frames, ground, pipe, and mountain background. Paid pack adds variants and Animate source. | Personal/commercial game use allowed. Standalone redistribution/resale barred. Reduce to three selected flight frames, then map the other sprites locally. |
| [Flappy Pigeon, overcrafted](https://overcrafted.itch.io/flappy-pigeon) | Pigeon, chicken, penguin, hen, city, pipes, and UI. Sprites-only ZIP or Unity package. Birds are 16 by 16. | Paid art pack. The inspected page lists the sale and contents without a complete usage license. Obtain and check the accompanying terms before use. |
| [Pixel Fluffy Birds, Yahallo Games](https://yahallogames.itch.io/pixel-fluffy-birds) | Animated fluffy bird pack with separate free and paid ZIPs. Archive layouts were not inspected. | Free version permits noncommercial projects. Paid version permits commercial projects. Both bar asset redistribution/resale, including modified files. |
| [Flappy Birds 16x16, Brysia](https://brysiaa.itch.io/pixel-flappy-birds-16x16px) | Bird species include duck, penguin, raven, toucan, and kiwi. Three flight frames plus a death pose, supplied as separate PNGs. | Personal/commercial use and edits allowed. Sharing or reselling source files barred. Combine a chosen species with separately authorized scenery and UI. |

These packs have different licenses. Keep each pack's downloaded terms and attribution with the private input files. A paid download does not establish permission for every distribution format. A public source repository can link the author's page without copying the pack into a release.

## Preparing a local import

The firmware accepts normalized local sprite folders. Each optional pack needs a local mapping into the expected filenames, dimensions, three flight frames, obstacle tiles, and UI entries. Check [asset instructions](../assets/README.md) for the current import commands and exact contract. Some character-only packs also need scenery you draw or obtain separately.

A catalog entry is a research pointer. It does not make its download archive directly compatible. The internal layouts of the listed downloads were not extracted or tested during this research. Any compatibility adapter must validate the actual files before claiming support.

For pixel work, select frames and draw missing tiles at the target pixel grid. Nearest-neighbor scaling preserves an existing grid, though shrinking detailed artwork can erase readable shapes. Keep transparent margins and collision masks consistent with the displayed character. The large mode should scale the selected art and its collision geometry together.

## Two new themes

**Toast on the Run:** a flying slice of toast with butter-colored wings. Toaster slots form the obstacles. Jam-colored hills and a crumb trail carry the joke without borrowing another game's artwork. A crash can drop a few square crumbs.

**Past Due:** a paper invoice flaps its folded corners through filing cabinets. Rubber stamps and paper stacks form the scenery. Its score counts successful escapes from paperwork. The invoice shape and lettering should be drawn from scratch.

The project-created themes need their own character silhouettes, obstacle art, backgrounds, and UI. Tinting another game's sprites would preserve their underlying expression.

## Rights confirmation copy

Use one unchecked confirmation during import/build setup:

> I confirm I have the rights or permission needed to use and convert the assets I supply for this build.

A short adjacent sentence can explain: "This confirmation does not grant asset rights." Keep the full [rights boundary](RIGHTS.md) available behind a link. Original game archives and third-party branding remain outside the public package.

## Excluded candidate

[Tiny Dragon Fly](https://alb-pixel-store.itch.io/tiny-dragon-fly-assets) has useful character and world sets, but the inspected terms prohibit physical-product use and projects that use generative AI in any form. This project has AI-assisted development and targets a physical device. The pack is excluded from the ten recommended candidates pending separate permission from its author.
