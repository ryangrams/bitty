#!/usr/bin/env python3
"""Create an audited ZIP containing only the explicit public source files."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
import tempfile
import zipfile

from check_distribution import DistributionError, ROOT, audit


def package(root: Path = ROOT, output: Path | None = None, check_git: bool = False) -> Path:
    root = root.resolve()
    files = audit(root, check_git)
    output = output or root / ".local" / "releases" / "m5-flap-source.zip"
    output = output.resolve()
    if output.suffix.casefold() != ".zip":
        raise DistributionError("Source ZIP destination must have a .zip extension")
    if output == root or root in output.parents and output.relative_to(root).as_posix() in files:
        raise DistributionError("Source ZIP destination would overwrite a public source file")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, prefix=".source-", suffix=".zip", delete=False) as handle:
            temporary = Path(handle.name)
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for name in sorted(files):
                info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = (0o100644 << 16)
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, files[name])
        temporary.replace(output)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT, help="Project directory to package")
    parser.add_argument("--output", type=Path, help="ZIP destination, default: .local/releases/m5-flap-source.zip")
    parser.add_argument("--git", action="store_true", help="Require and audit Git index and all reachable history")
    args = parser.parse_args()
    try:
        output = package(args.root, args.output, args.git)
    except (DistributionError, UnicodeDecodeError, OSError) as error:
        print(f"Source package failed: {error}", file=sys.stderr)
        return 1
    print(f"Source ZIP: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
