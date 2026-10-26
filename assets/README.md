# Supply compatible assets locally

Use artwork and audio you created, obtained under a suitable license, or otherwise have the right to use and convert for your intended build. The public package includes code for two project-created procedural themes. It excludes imported game artwork and audio.

The import tools read local files and make no network requests. They validate file structure and record your rights confirmation. They cannot verify ownership or grant permission. See [rights and distribution](../docs/RIGHTS.md).

## Native importer and confirmation

```sh
python tools/import_gui.py
```

Use Python with Tkinter, the project requirements, and a desktop display. Choose a local APK or an atlas PNG with its text manifest. Choose an optional audio folder and a private output folder, then check:

> I confirm I have the rights or permission needed to use and convert the assets I supply for this build.

The box starts unchecked. Import becomes available after you check it. This confirmation records your statement and grants no license. The command-line tools use `--acknowledge-rights` for the same statement.

## Atlas and manifest

```sh
python tools/generate_assets.py --atlas /path/to/atlas.png --manifest /path/to/atlas.txt --acknowledge-rights
```

The atlas uses the named entries and source dimensions in `REQUIRED_SIZES` in [the importer](../tools/generate_assets.py). Each manifest line has seven whitespace-separated fields:

```text
name width height u v normalized_width normalized_height
```

Coordinates start at the PNG's upper-left corner and are normalized by its dimensions. Sprites must contain exact duplicated 2 by 2 pixel blocks. The importer validates dimensions, boundaries, transparency, and that grid before producing RGB565 data.

The format supports the classic Android atlas layout. You can draw a compatible atlas using those entry names and dimensions. The tests' `make_fixture()` creates independent geometric examples for verification, with no original sprites or logo.

## Optional local APK

```sh
python tools/generate_assets.py --apk /path/to/your-copy.apk --acknowledge-rights
```

The file must be a compatible, unencrypted ZIP-format APK containing `atlas.png` and `atlas.txt`. Standard `assets/atlas.png` and `assets/atlas.txt` paths are preferred. Otherwise, a unique matching basename is accepted. Unsupported formats fail with a diagnostic. The importer reads selected entries without running the APK, installing it, or extracting arbitrary archive paths.

Your permission must cover the extraction, conversion, and use you plan. This project provides no APK download link. An archived copy does not establish authorization.

## Optional audio

Add `--audio-dir /path/to/audio` to either base-import command. Supported effect names are `wing`, `point`, `hit`, `die`, and `swoosh`, with `.ogg` or `.wav` extensions. A `sfx_` prefix is also accepted. Compatible sounds inside an APK are read automatically. FFmpeg converts supplied audio to 16 kHz signed 16-bit mono PCM. Missing effects are silent.

## Custom skin folders

The firmware provides ten palette presets, including Original, and two project-created themes: Toast on the Run and Past Due. You can add up to ten local custom packs. The [curated source catalog](skin_catalog.json) points to ten author packs with different rights. Those recommendations are separate from the palette presets. Their downloadable archives need preparation before import.

Prepare each chosen skin in its own private folder with a UTF-8 `skin.json` and local images. Three bird frames are required. Other images are optional:

| Key | Normalized pixels | Required |
| --- | --- | --- |
| `birdUp` | 17 by 12 | Yes |
| `birdMid` | 17 by 12 | Yes |
| `birdDown` | 17 by 12 | Yes |
| `background` | 144 by 256 | No |
| `ground` | 168 by 56 | No |
| `pipe` | 26 by 160 | No |
| `pipeTop` | 26 by 160 | No |

For example:

```json
{
  "name": "My local skin",
  "credit": "[TK: artist credit]",
  "license": "[TK: applicable license or permission]",
  "files": {
    "birdUp": "bird-up.png",
    "birdMid": "bird-mid.png",
    "birdDown": "bird-down.png",
    "background": "background.png",
    "ground": "ground.png",
    "pipe": "pipe.png",
    "pipeTop": "pipe-top.png"
  }
}
```

`source_url` is optional text metadata. Image paths must stay inside the skin folder. Keep that folder and its manifest private. Missing optional images use the base artwork. Supply both `pipe` and `pipeTop` to replace both obstacle directions. The importer resizes images to the dimensions above with nearest-neighbor sampling. Prepare them at the target pixel grid when exact shapes matter. It derives each pack's collision masks from its three bird frames.

Repeat `--skin-dir` for each pack, up to ten:

```sh
python tools/generate_skins.py --skin-dir /path/to/first-skin --skin-dir /path/to/second-skin --acknowledge-rights
python tools/build.py --skins-dir .local/skins
```

Skin output defaults to ignored `.local/skins/`. Use `--output-dir` to select another private destination, then pass that directory to the build with `--skins-dir`. Artist credit, license text, source metadata, input hashes, and the rights confirmation are retained in the private import receipt. They do not make distribution of the resulting assets or firmware automatically permissible.

## Private output

Base imports default to ignored `.local/assets/`. `--output-dir` can select another private destination; pass it to `tools/build.py` with `--assets-dir`.

Base output includes `generated_assets.h`, `generated_sounds.h`, and `generated_collision.h`. Custom skin output includes `generated_skins.h`. Input copies, normalized images, collision data, and import receipts stay in those private output folders. Generated headers and firmware can reproduce the same protected expression as their inputs.

The build stages these files under ignored `.local/sketch/`. They stay outside tracked firmware source and public release packages. Read each pack's downloaded license before distributing any asset-bearing build.
