# Build Bitty with an agent

Use this checkout with Codex, Claude Code or another coding agent that can read files and run a shell. Start with an original game or a small improvement, then follow [Creating games and apps](CREATING_GAMES.md). The repository's historical `firmware/flappy/` folder contains the shared Bitty system.

## Set up a checkout

Install Git, Python 3.11 or newer, a C++11 compiler, Arduino CLI and Node.js. Arduino dependencies are pinned by [tools/setup.py](../tools/setup.py). Node runs the browser transport tests. Optional media import tools have extra requirements in the [README](../README.md).

```sh
git clone https://github.com/ryangrams/bitty.git
cd bitty
python3 -m venv .local/venv
. .local/venv/bin/activate
python -m pip install -r requirements.txt
python tools/setup.py
```

On Windows, activate `.local\venv\Scripts\activate` in your shell. Keep imported game files and generated output under ignored `.local/`.

Open this folder in Codex or start your installed Codex, Claude Code or other CLI agent from the checkout root. Each client keeps its own account and installation setup. The prompt below uses explicit file paths, so automatic skill discovery is optional.

## Give the agent the project rules

The public skills are ordinary `SKILL.md` files with supporting references:

| Skill | Use it for |
| --- | --- |
| [M5Stick development](../skills/m5stick-development/SKILL.md) | Hardware identity, timing, display ownership, controls, audio and sleep. |
| [Game fidelity](../skills/m5stick-game-fidelity/SKILL.md) | Comparing a port with a supplied reference and recording adaptations. |
| [Content to GitHub](../skills/content-to-github/SKILL.md) | Public source, browser installation, release evidence and community routing. |

Ask Codex or Claude to read these files directly. Other agents can follow the same instructions as repository documentation. If your client supports installing skill folders, preserve each folder's `references/` files and use that client's installation mechanism. Repository links resolve inside a full Bitty checkout; an independently installed skill can use the [public home contract](https://github.com/ryangrams/bitty/blob/main/docs/GAME_HOMES.md) and [authoring guide](https://github.com/ryangrams/bitty/blob/main/docs/CREATING_GAMES.md).

Paste a scoped task such as:

```text
Work in this Bitty checkout. Read CONTRIBUTING.md, docs/CREATING_GAMES.md,
docs/USB_LIBRARY.md, docs/RELEASE_STATUS.md and docs/RIGHTS.md first.
Read skills/m5stick-development/SKILL.md and its hardware reference.
Use the game-fidelity skill for any reference-based work.

Build an original [game or app idea] for the original M5StickC Plus.
Keep its simulation independent of rendering. Provide a portrait animated
clock home using game_home.h and clock_runtime.h. Left tap selects the next
available game on home; front M5 starts play. Keep the shared menu controls.
Use original or suitably licensed code, art and audio.

Inspect the existing integration points before editing. Keep private game
files out of source and tests. Run the source tests, compile the exact board
target and report what was tested on a host versus on physical hardware.
Do not flash, publish or change partitions unless that is part of this task.
```

Replace the bracketed idea before sending. Identify the exact board and intended scope in follow-up requests. Keep working hardware settings and user records intact during ordinary updates.

## Check the result

```sh
python tools/test.py
node tests/web_protocol_test.mjs
python tools/build.py --public --library --display stickc-plus
python tools/check_distribution.py --git
python tools/package_source.py --git
```

For emulator changes, also run `python tools/test_nes.py`. The source exporter includes only reviewed paths in [PUBLIC_FILES.txt](../PUBLIC_FILES.txt). A new source or documentation file needs an entry before packaging. Use a full Git checkout for the history audit.

The public build uses project-authored inputs and avoids local ROM/APK discovery. A successful host test or compile establishes a software check. Record separate device evidence for controls, completed display transfers, sound and battery wake. Check [release status](RELEASE_STATUS.md) before describing a browser installer or hardware target as verified.

Adding native C++ code requires a firmware build. The USB library currently accepts compatible user cartridge files; it does not execute uploaded native plug-ins. See [the authoring guide](CREATING_GAMES.md) before designing a downloadable game package.
