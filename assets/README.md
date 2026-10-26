# Supply compatible assets locally

The public repository contains no game artwork or audio. Use assets you created, received under a suitable license, or otherwise have the right to use and convert for your intended build. The importer accepts compatible local files and never downloads them. It does not verify ownership or grant permission.

## Atlas and manifest

```sh
python tools/generate_assets.py --atlas /path/to/atlas.png --manifest /path/to/atlas.txt
```

The PNG atlas uses the named entries and source dimensions in `REQUIRED_SIZES` in [the importer](../tools/generate_assets.py). Each manifest line has seven whitespace-separated fields:

```text
name width height u v normalized_width normalized_height
```

Coordinates are measured from the PNG's upper-left corner and normalized by the atlas dimensions. Sprites must contain exact duplicated 2 by 2 pixel blocks. The importer validates dimensions, boundaries, transparency, and that pixel grid before producing RGB565 data.

The format supports the classic Android atlas layout. You can draw your own compatible atlas using those entry names and dimensions. The tests' `make_fixture()` creates independent geometric examples for build verification. Those examples are test data, with no original sprites or logo.

## Optional local APK

```sh
python tools/generate_assets.py --apk /path/to/your-copy.apk
```

The file must be a compatible, unencrypted ZIP-format APK containing `atlas.png` and `atlas.txt`. Standard `assets/atlas.png` and `assets/atlas.txt` paths are preferred. Otherwise a unique matching basename is accepted. Unsupported formats fail with a diagnostic. The importer reads selected entries without running the APK, installing it, or extracting arbitrary archive paths.

Only use this route when you have the relevant rights for the extraction, conversion, and use. This project provides no APK download link and makes no claim that an archived copy is authorized. See [rights and distribution](../docs/RIGHTS.md).

## Optional audio

Add `--audio-dir /path/to/audio` to either command. Supported effect names are `wing`, `point`, `hit`, `die`, and `swoosh`, with `.ogg` or `.wav` extensions. A `sfx_` prefix is also accepted. Compatible sounds inside an APK are read automatically. FFmpeg converts supplied audio to 16 kHz signed 16-bit mono PCM. Missing effects are silent.

## Private output

The default destination is ignored `.local/assets/`. `--output-dir` can select another local destination. Keep that directory outside public commits and release packages.

Output includes `generated_assets.h`, `generated_sounds.h`, and `generated_collision.h`. Collision masks are derived from the supplied bird alpha channel, so the public gameplay core contains no original bird silhouette. Input copies, sprite crops, and an import receipt also stay in the private output directory. Generated headers and firmware can contain the same protected expression as their inputs.

The build copies these headers into an ignored staged sketch. It never copies them into the tracked firmware source directory.
