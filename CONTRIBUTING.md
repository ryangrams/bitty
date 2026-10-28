# Build something Bitty

Original games and useful little apps are welcome. A clear control scheme and a playable small version are a good start. Ports, device support, timing measurements, accessibility, documentation and test reports all help.

Open a game/app request before a large change so the device limits and public distribution plan are clear. Explain the controls, source/license, reference behavior and smallest useful version. Keep commercial ROMs, APKs, extracted artwork, personal information and machine-specific captures out of issues and pull requests.

Run `python tools/test.py` and `node tests/web_protocol_test.mjs`. For emulator changes, also run `python tools/test_nes.py`. Use `python tools/check_distribution.py --git` and the source packager before publishing. Add reviewed source paths to `PUBLIC_FILES.txt`. Public fixtures must be synthetic or redistributable.

Use the skills under `skills/` for M5Stick development, game fidelity audits and content-to-GitHub publishing. They use the same SKILL.md format for Codex and Claude. Read the hardware evidence before reusing board settings. Separate host simulation, injected device input and physical-controller evidence in reports.

Two original game directions are under consideration: a summer-camp flashlight-dodging game using tilt and a duck button, and a cable-routing puzzle about a gremlin moving signals between patch points. These are proposals. Names, designs and implementation remain open for contributors.

If you enjoy the project, a voluntary GitHub star is welcome. You can also support future builds through [GitHub Sponsors](https://github.com/sponsors/ryangrams). Support and stars are optional.
