#!/usr/bin/env python3
"""Audit the explicit source publication boundary and optional Git history."""

from __future__ import annotations

import argparse
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = "PUBLIC_FILES.txt"
MAX_FILE_BYTES = 256 * 1024
TEXT_SUFFIXES = {".py", ".h", ".ino", ".cpp", ".md", ".txt", ".yml", ".yaml", ".sh"}
SPECIAL_NAMES = {"LICENSE", ".gitignore", ".gitattributes"}
PRIVATE_PARTS = {
    ".git", ".local", ".pio", "__pycache__", "artifacts", "build", "build-probe",
    "captures", "data", "downloads", "node_modules", "previews", "private",
    "screenshots", "source-assets", "toolchain",
}
WINDOWS_DEVICES = {"con", "prn", "aux", "nul"} | {
    f"{prefix}{number}" for prefix in ("com", "lpt") for number in range(1, 10)
}
MEDIA_MAGIC = (
    bytes.fromhex("89504e470d0a1a0a"), b"GIF87a", b"GIF89a", bytes.fromhex("ffd8ff"),
    b"OggS", b"RIFF", b"fLaC", b"ID3", bytes.fromhex("7f454c46"), b"PK\x03\x04",
)
MACHINE_PATH = re.compile(rb"/(?:Users|Volumes|home)/[^\s\"'<>]+|usbserial-[A-Za-z0-9]{6,}")
ORIGINAL_MASK = re.compile(rb"\b0x[fF]{3}000ULL\b")
NUMERIC_LITERAL = rb"(?:0x[0-9a-fA-F]+|[0-9]+)(?:[uU](?:[lL]{1,2})?|[lL]{1,2}[uU]?)?"
DENSE_ARRAY = re.compile(NUMERIC_LITERAL + rb"(?:\s*,\s*" + NUMERIC_LITERAL + rb"){63,}")
MIRROR_URL = re.compile(rb"https?://(?:gist\.githubusercontent\.com/|[^\s]+/samuelcust/flappy-bird-assets/)")


class DistributionError(Exception):
    """The candidate source package contains a publication boundary violation."""


def path_problem(name: str) -> str | None:
    path = PurePosixPath(name)
    if not name or "\\" in name or path.is_absolute() or path.as_posix() != name:
        return "use a normalized relative POSIX path"
    if any(part in {".", ".."} for part in name.split("/")):
        return "path traversal is forbidden"
    if any(ord(char) < 32 or char in '<>:"|?*' for char in name):
        return "path contains characters unsupported in portable source ZIPs"
    if any(part.endswith((" ", ".")) or part.split(".", 1)[0].casefold() in WINDOWS_DEVICES for part in path.parts):
        return "path is incompatible with Windows filenames"
    parts = [part.casefold() for part in path.parts]
    if any(part in PRIVATE_PARTS for part in parts):
        return "private or generated directory"
    if parts[:2] == ["assets", "source"]:
        return "local asset source directory"
    if "generated" in path.name.casefold() or path.name.casefold() in {"atlas.txt", "atlas.json"}:
        return "generated asset data or original asset manifest"
    if path.name not in SPECIAL_NAMES and path.suffix.casefold() not in TEXT_SUFFIXES:
        return "only source code, text documentation and source configuration are allowed"
    return None


def parse_manifest(data: bytes, label: str = MANIFEST) -> list[str]:
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as error:
        raise DistributionError(f"{label}: manifest must be UTF-8 text") from error
    names = []
    for line_number, line in enumerate(text.splitlines(), 1):
        name = line.strip()
        if not name or name.startswith("#"):
            continue
        problem = path_problem(name)
        if problem:
            raise DistributionError(f"{label}:{line_number}: {name}: {problem}")
        if name.casefold() in {existing.casefold() for existing in names}:
            raise DistributionError(f"{label}:{line_number}: duplicate entry: {name}")
        names.append(name)
    if MANIFEST not in names:
        raise DistributionError(f"{label}: include {MANIFEST} in the publication list")
    return names


def content_problem(data: bytes) -> str | None:
    if len(data) > MAX_FILE_BYTES:
        return f"file exceeds the {MAX_FILE_BYTES}-byte source limit"
    if any(data.startswith(magic) for magic in MEDIA_MAGIC):
        return "media, archive or executable content"
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        return "file must contain UTF-8 text"
    if any(ord(char) < 32 and char not in "\n\r\t" for char in text):
        return "binary control characters in text"
    if MACHINE_PATH.search(data):
        return "machine-specific path or device identity"
    if ORIGINAL_MASK.search(data):
        return "original bird silhouette data"
    if DENSE_ARRAY.search(data):
        return "dense numeric data requires manual review outside the source package"
    if MIRROR_URL.search(data):
        return "original asset mirror URL"
    return None


def read_public_files(root: Path) -> dict[str, bytes]:
    root = root.resolve()
    manifest_path = root / MANIFEST
    if manifest_path.is_symlink():
        raise DistributionError(f"{MANIFEST}: symlinks are forbidden")
    try:
        manifest_data = manifest_path.read_bytes()
    except OSError as error:
        raise DistributionError(f"{MANIFEST}: cannot read publication list") from error
    problem = content_problem(manifest_data)
    if problem:
        raise DistributionError(f"{MANIFEST}: {problem}")
    names = parse_manifest(manifest_data)
    files = {}
    for name in names:
        path = root / name
        cursor = root
        for part in PurePosixPath(name).parts:
            cursor /= part
            if cursor.is_symlink():
                raise DistributionError(f"{name}: symlinks are forbidden")
        try:
            path.resolve().relative_to(root)
        except ValueError as error:
            raise DistributionError(f"{name}: path leaves the project") from error
        if not path.is_file():
            raise DistributionError(f"{name}: missing regular source file")
        try:
            with path.open("rb") as source:
                data = source.read(MAX_FILE_BYTES + 1)
        except OSError as error:
            raise DistributionError(f"{name}: cannot read source file") from error
        problem = content_problem(data)
        if problem:
            raise DistributionError(f"{name}: {problem}")
        files[name] = data
    return files


def git(root: Path, *arguments: str, input_data: bytes | None = None) -> bytes:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), *arguments], input=input_data,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        )
    except OSError as error:
        raise DistributionError("Git is required for the history audit") from error
    if result.returncode:
        raise DistributionError(f"Git audit failed while running {arguments[0]}")
    return result.stdout


def audit_git(root: Path, current_names: set[str]) -> None:
    top = Path(git(root, "rev-parse", "--show-toplevel").decode().strip()).resolve()
    if top != root.resolve():
        raise DistributionError("Git audit requires a repository rooted at the project")
    if git(root, "rev-parse", "--is-shallow-repository").strip() == b"true":
        raise DistributionError("Git history audit requires a complete clone; fetch the full history")
    blobs: set[str] = set()
    staged = git(root, "ls-files", "--stage", "-z")
    for entry in staged.split(b"\0"):
        if not entry:
            continue
        metadata, raw_name = entry.split(b"\t", 1)
        mode, object_id, stage = metadata.decode().split()
        name = raw_name.decode("utf-8", errors="strict")
        if name not in current_names:
            raise DistributionError(f"Git index: {name}: outside {MANIFEST}")
        if mode not in {"100644", "100755"} or stage != "0":
            raise DistributionError(f"Git index: {name}: only resolved regular files are allowed")
        blobs.add(object_id)

    commits = git(root, "rev-list", "--all").decode().splitlines()
    for commit in commits:
        tree = {}
        for entry in git(root, "ls-tree", "-r", "-z", commit).split(b"\0"):
            if not entry:
                continue
            metadata, raw_name = entry.split(b"\t", 1)
            mode, kind, object_id = metadata.decode().split()
            name = raw_name.decode("utf-8", errors="strict")
            problem = path_problem(name)
            if problem:
                raise DistributionError(f"Git history {commit[:12]}: {name}: {problem}")
            if kind != "blob" or mode not in {"100644", "100755"}:
                raise DistributionError(f"Git history {commit[:12]}: {name}: regular files required")
            tree[name] = object_id
            blobs.add(object_id)
        if MANIFEST not in tree:
            raise DistributionError(f"Git history {commit[:12]}: missing {MANIFEST}")
        historical_names = set(parse_manifest(
            git(root, "cat-file", "blob", tree[MANIFEST]), f"Git history {commit[:12]} {MANIFEST}"
        ))
        if set(tree) != historical_names:
            raise DistributionError(f"Git history {commit[:12]}: tree differs from its publication list")

    for line in git(root, "rev-list", "--objects", "--all").splitlines():
        if line:
            blobs.add(line.split(b" ", 1)[0].decode())
    if not blobs:
        return
    object_ids = sorted(blobs)
    details = git(root, "cat-file", "--batch-check", input_data=("\n".join(object_ids) + "\n").encode())
    for line in details.decode().splitlines():
        object_id, kind, raw_size = line.split()
        if kind != "blob":
            continue
        if int(raw_size) > MAX_FILE_BYTES:
            raise DistributionError(f"Git object {object_id[:12]}: exceeds the source size limit")
        problem = content_problem(git(root, "cat-file", "blob", object_id))
        if problem:
            raise DistributionError(f"Git object {object_id[:12]}: {problem}")


def audit(root: Path = ROOT, check_git: bool = False) -> dict[str, bytes]:
    root = root.resolve()
    files = read_public_files(root)
    if check_git or (root / ".git").exists():
        audit_git(root, set(files))
    return files


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT, help="Project directory to audit")
    parser.add_argument("--git", action="store_true", help="Require and audit Git index and all reachable history")
    args = parser.parse_args()
    try:
        files = audit(args.root, args.git)
    except (DistributionError, UnicodeDecodeError) as error:
        print(f"Source audit failed: {error}", file=sys.stderr)
        return 1
    print(f"Source audit passed: {len(files)} allowlisted text files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
