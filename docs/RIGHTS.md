# Rights and distribution

M5 Flap distributes firmware source, build tools and documentation under the [project license](../LICENSE), subject to the [upstream notices](../THIRD_PARTY_NOTICES.md). Users supply compatible artwork and optional audio locally. This project cannot grant permission to use another game's assets.

## Public source boundary

Keep imported atlases, sprite crops, sounds, sprite-derived collision masks, generated asset headers, screenshots, recordings, firmware binaries and device backups out of public commits, Git history, release attachments and CI artifacts. These files can contain or reproduce third-party expression even when their file format changes. Public exports must also exclude installed dependencies and local toolchains.

The public tools accept local input files. They do not download original game assets, locate archived APKs, bypass access controls or establish an asset license. Build output containing imported material stays local unless its distribution is separately authorized.

## Supplying assets

Use artwork and audio you created, obtained under a suitable license, or otherwise have the legal right to reproduce and use for the intended build. Check whether your permission covers any conversion, extraction and distribution you plan. A copy of a game, an uploader's credit, or a repository's software license does not by itself establish those rights.

Internet Archive availability also does not establish permission. The Archive says it [does not guarantee the copyright status of items or the accuracy of posted rights information](https://help.archive.org/help/rights/). Evaluate the work's rights and your intended use before importing files. The project provides no original-asset download source.

## What this separation establishes

The U.S. Copyright Office [distinguishes a game's ideas and methods from copyrightable expression](https://www.copyright.gov/register/tx-games.html). Copyright can protect artwork, text, sound recordings and audiovisual material. [17 U.S.C. Section 106](https://www.govinfo.gov/content/pkg/USCODE-2024-title17/html/USCODE-2024-title17-chap1-sec106.htm) reserves reproduction, derivative-work and distribution rights to the copyright owner, subject to statutory exceptions.

Separating source from assets reduces what the repository distributes. It does not establish legal clearance for every clone, imported asset or finished build. Courts have also found infringement in copied game presentation, as in [Tetris Holding v. Xio Interactive, D.N.J. 2012](https://www.govinfo.gov/content/pkg/USCOURTS-njd-3_09-cv-06115/pdf/USCOURTS-njd-3_09-cv-06115-0.pdf). Name and branding choices raise separate [trademark confusion questions](https://www.uspto.gov/trademarks/search/likelihood-confusion).

These are the project's distribution boundaries, based on U.S. sources. They are not a legal opinion or a guarantee of clearance. A rights holder's authorization or qualified legal review is needed to resolve uncertainty about a particular distribution.
