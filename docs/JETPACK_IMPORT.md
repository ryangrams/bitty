# Private Jetpack reference import

You can rebuild Jetpack with coin layouts from an Android package you supply. The public game includes authored formations and artwork, so it also builds without this import. Use a package you have the rights to use and transform. The importer requires an explicit acknowledgement. It records your confirmation and grants no rights.

```sh
python3 tools/import_jetpack.py "/path/to/your/game.apkm" --accept-rights
python3 tools/build.py --jetpack-dir .local/jetpack/imported
```

The importer supports the observed APKM install pack, an APK containing `assets/assets.zip`, a direct APK asset directory, or the game asset ZIP itself. It reads selected nested ZIP members in memory. It does not install or execute the Android application.

The default output is `.local/jetpack/imported/`. Keep this folder private and out of Git. It contains:

- `jetpack_imported.h`: packed coin coordinates, formation metadata and optional Rocket Time waves for the firmware.
- `receipt.json`: source and selected-file hashes, formation counts, dimensions, and import options.
- `reference-config.json`: decoded settings and vehicle configuration for private reference. Importing these values does not automatically change the game's physics.

The build uses this default folder when its header exists. Pass `--no-jetpack-import` to build with authored formations and artwork. You can choose another private folder with `--output` during import and `--jetpack-dir` during the build. Outputs inside the repository must remain under `.local/`.

## Coin layouts

The importer preserves every `1` in each supported ASCII formation. A `0` or space is an empty cell, and blank rows keep their vertical spacing. Coordinates are sorted by column and then row so the game can draw the visible part of a large formation. The original grid dimensions remain in the header. The game chooses the on-screen grid spacing for this display.

Classic eligibility and relative weights come from the package's base coin configuration. The receipt reports missing file references without guessing filename aliases. Those references do not enter the classic selection. Event-tagged formations remain in the private data without entering the ordinary classic selection. Cash Dash and onboarding filenames receive separate flags. Including a formation in the header does not enable every original event or vehicle sequence.

When the package includes Teleporter, SAM, Coin Streak, Gravity Suit or Mech coin profiles, the importer also preserves their selection weights and distance steps. Empty formations remain valid blank choices. The firmware uses the current profile's distance when selecting these layouts. The receipt lists which profiles were found and reports missing references separately. These records select layouts; they do not import the original program or enable unrelated events.

The importer rejects oversized archives, duplicate or unsafe paths, unsupported cells, malformed configuration, and invalid dimensions. Patterns are limited to 256 columns, 40 rows, and 512 occupied cells each, with a combined maximum of 65,535 occupied cells. The row limit keeps every coin within the display's vertical play area at an integer pixel pitch. It reports unsupported data and preserves an existing import when validation fails.

## Optional Rocket Time waves

When the supplied package contains supported Rocket Time missile configuration, the importer also stores its pattern weights, lanes or lane ranges, angles, speeds, direction flags, chase speed and warning/launch timing in the private header. The receipt records the number of patterns and missiles. The package used during this audit supplied 15 patterns and 88 missile entries.

These records remain private. The public source contains authored fallback waves and tests built from synthetic data. Older imported headers without wave metadata still use the fallback. Reimporting a compatible package adds its optional metadata without requiring original runner art. Missing or malformed optional configuration is handled by the importer's validation rules. See [the audit](JETPACK_AUDIT.md) for Rocket Time behavior and the remaining adaptations.

## Optional original runner

The normal import keeps the project's authored runner, vehicles, scientists, and scenery. To make a separate private pack that fits the supplied runner layers to the M5 screen:

```sh
python3 tools/import_jetpack.py "/path/to/your/game.apkm" \
  --accept-rights --original-runner --output .local/jetpack/imported-original
python3 tools/build.py --jetpack-dir .local/jetpack/imported-original
```

This option combines the package's default head, body, and jetpack with their layer offsets, then fits their shared bounds into 20 by 15 pixels with nearest sampling. It produces six frames: four run poses and two flight poses. The 32-pixel cell interpretation and pose mapping are approximations based on the supplied sheets. Crash poses, vehicles, scientists, and the rest of the scene use authored artwork. The receipt records this limitation and the source image hashes. Changing the original game package may require updating the supported texture format or frame mapping.

The generated header stores RGB565 pixels with `0xf81f` reserved for transparency. The firmware can flip these frames vertically for reversed gravity. Original image files and audio are not copied into the public project or source package.

Run `python3 -m unittest discover -s tests -p test_import_jetpack.py -v` to check the importer using synthetic data. The tests use Python's standard library. A C++11 compiler, when available, also checks both generated header variants.
