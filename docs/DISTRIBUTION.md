# Prepare a source release

The public release contains the files named in [PUBLIC_FILES.txt](../PUBLIC_FILES.txt): firmware source, project-created procedural drawing code, import/build tools, synthetic tests, licenses, documentation, and a metadata-only skin catalog. Imported media and asset-bearing firmware stay private.

## Run the release checks

From a full Git checkout:

```sh
python tools/test.py
python tools/check_distribution.py --git
python tools/package_source.py --git
```

The exporter writes `.local/releases/m5-flap-source.zip`. With an extracted source ZIP, omit `--git`. A checkout is audited automatically even when that flag is absent. The audit rejects shallow history because it cannot inspect missing commits.

The test suite creates independent geometric fixtures. It exercises the local importers, game rules, menus and automated modes, rendering, co-op protocol, and publication checks. It does not require original game media or publish screenshots, generated headers, sound, or firmware. A passing protocol test does not establish working Bluetooth behavior between two devices. Follow the physical test list in [COOP.md](COOP.md), including pairing, launch timing, map agreement, each player's controls, dropout, and rematch.

## What the audit checks

The exporter reads an explicit allowlist. New public source files need an entry there before they enter a release. The current file checks require normalized portable paths, regular files, UTF-8 text, bounded file sizes, and recognized source/document extensions. They reject symlinks, private/generated paths, media signatures, executable/archive signatures, machine-specific paths, known original-asset mirror URLs, and dense numeric data that needs manual review.

JSON is allowed only at `assets/skin_catalog.json`, the reviewed metadata catalog. Original atlas manifests, local `skin.json` inputs, import receipts, generated headers, PNG files, and audio remain excluded. Adding `.json` support does not open the package to arbitrary asset JSON.

For Git repositories, the audit inspects staged files and every reachable commit. Each historical tree must match its own publication list. Blob content is checked across branches and tags, including a media blob attached directly to a tag. Removing a leaked file in a later commit leaves its earlier copy in history.

These checks support review. They cannot recognize every copied asset, prove authorship, validate an uploader's rights, or guarantee legal clearance. Review the [rights boundary](RIGHTS.md), [upstream notices](../THIRD_PARTY_NOTICES.md), source changes, and the contents of the final ZIP before sharing it.

## Keep these files private

Imported atlases and manifests, custom skin folders, sounds, normalized images, sprite-derived collision masks, generated data, screenshots, recordings, firmware binaries, device backups, downloaded dependencies, and local toolchains belong outside public commits and release attachments. The import/build commands use ignored `.local/` directories by default.

A local rights checkbox records the builder's confirmation. It grants no rights and cannot authorize redistribution of an asset pack or a finished build. Archive.org availability and ownership of a game copy also leave the intended-use question unresolved.

The source package can include the ten project palette definitions and two original procedural themes. The ten researched author packs contribute source links and rights summaries only. Each pack's files and any firmware built with them require their own distribution review.

## Release contents and cleanup

Use the exporter for a source attachment. Creating a ZIP of the whole working folder can sweep in private inputs and output. Keep any asset-bearing firmware separate until you have permission for its distribution.

Build caches and test executables under `.local/build/` and `.local/tests/` can be deleted and recreated with `python tools/build.py` and `python tools/test.py`. Staged source under `.local/sketch/` is recreated by the build. Preserve private asset inputs, import receipts, and device backups that you cannot recover before removing larger parts of `.local/`. This note records which output is reproducible and how to restore it.
