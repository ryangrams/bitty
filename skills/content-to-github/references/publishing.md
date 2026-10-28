# Publishing decisions

## Repository and installation

Keep a small resident system plus explicitly supported user payloads. Store cartridges outside the app image if deleting one must recover flash. Use atomic directories, payload checksums, bounded transfer chunks and interrupted-install tests. First installation and runtime management are separate protocols even when shown on one site.

A GitHub Pages site can serve static HTTPS installation and browser code. Release assets must contain only distributable firmware. Build in a clean checkout with authored assets and no automatic discovery of a developer's private ROM directories. Verify the board and flash size. Keep a recovery download and serial instructions.

Use an allowlist for public source and audit all Git history before the first push. Keep provenance, license notices and import instructions. Do not include cartridge binaries in issue templates or ask reporters to upload commercial files.

## Project support

Official references, recheck before publication:
- GitHub Pages limits: https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits
- GitHub acceptable use: https://docs.github.com/en/site-policy/acceptable-use-policies/github-acceptable-use-policies
- Sponsors enrollment: https://docs.github.com/en/sponsors/receiving-sponsorships-through-github-sponsors/about-github-sponsors-for-open-source-contributors
- Amazon identification: https://affiliate-program.amazon.com/help/node/topic/GHQNZAU6669EZS98
- ESP Web Tools: https://esphome.github.io/esp-web-tools/
- Espressif browser flasher: https://github.com/espressif/esptool-js

Pages should serve the open-source project. Keep support and project-related shopping optional and secondary. No checkout, sensitive payment form or commercial service backend belongs on this static project site. A star request may be a plain optional link. Never exchange stars for gifts, access or prizes.

Amazon requires this identification: "As an Amazon Associate I earn from qualifying purchases." Put a plain affiliate disclosure beside the links too. Verify the owner's account/tag and exact supported product. Do not copy unsupported specs, fabricate stock/price or imply other M5 variants are compatible.

GitHub Sponsors needs owner enrollment and payout setup. Prepare FUNDING.yml with an active, verified account. Leave inactive configuration disabled and explain the owner action. No claim of tax deductibility.

## Community

Bitty belongs in Studio Upgrade's gizmo/build area under the user's current direction. Start with a small category and three functions: share builds, get help, propose games/apps. Forum channels can keep each topic in a separate thread if the server supports them. Preserve existing moderation, roles and access. Avoid mass mentions and unsolicited announcements. GitHub Issues remains the tracked engineering queue. Add links only after verifying the invite and destination.

## Domain

The brand and registered domain are separate facts. Verify availability and both first-year and renewal pricing at the requested registrar. A missing DNS or RDAP record does not prove registrability. Do not invent contact information, accept new terms or purchase beyond authorization. Keep GitHub Pages usable while a domain is pending. Configure DNS and Pages domain verification before publishing the custom hostname.
