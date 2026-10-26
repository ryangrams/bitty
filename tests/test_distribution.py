"""Check that local artwork and historical leaks stay outside source releases."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_distribution import DistributionError, MAX_FILE_BYTES, audit, git
from package_source import package


class DistributionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.write("README.md", b"# Synthetic source fixture\n")
        self.manifest(["PUBLIC_FILES.txt", "README.md"])

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def manifest(self, names):
        self.write("PUBLIC_FILES.txt", ("\n".join(names) + "\n").encode())

    def git(self, *arguments):
        return subprocess.run(
            ["git", "-C", str(self.root), *arguments], check=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )

    def initialize_git(self):
        if not shutil.which("git"):
            self.skipTest("Git is unavailable")
        self.git("init")
        self.git("config", "user.name", "Source Audit Test")
        self.git("config", "user.email", "source-audit@example.invalid")

    def commit(self, message="Synthetic source fixture"):
        self.git("add", "--all")
        self.git("commit", "-m", message)

    def test_zip_uses_only_manifest_and_is_reproducible(self):
        self.write(".local/source/atlas.png", bytes.fromhex("89504e470d0a1a0a") + b"private")
        self.write("artifacts/firmware.bin", bytes(range(256)))
        first = package(self.root)
        first_bytes = first.read_bytes()
        second = package(self.root)
        self.assertEqual(first_bytes, second.read_bytes())
        with zipfile.ZipFile(second) as archive:
            self.assertEqual(archive.namelist(), ["PUBLIC_FILES.txt", "README.md"])
            self.assertEqual(archive.read("README.md"), b"# Synthetic source fixture\n")

    def test_rejects_private_and_generated_paths(self):
        for name in [".local/source/atlas.txt", "assets/source/atlas.txt", "firmware/flappy/generated_assets.h", "artifacts/log.txt"]:
            with self.subTest(name=name):
                self.manifest(["PUBLIC_FILES.txt", name])
                self.write(name, b"placeholder\n")
                with self.assertRaises(DistributionError):
                    audit(self.root)

    def test_reviewed_skin_catalog_metadata_is_included_in_source_zip(self):
        name = "assets/skin_catalog.json"
        metadata = b'{"schema_version":1,"packs":[{"name":"Synthetic skin","rights":"Synthetic fixture"}]}\n'
        self.write(name, metadata)
        self.manifest(["PUBLIC_FILES.txt", "README.md", name])
        self.assertEqual(audit(self.root)[name], metadata)
        with zipfile.ZipFile(package(self.root)) as archive:
            self.assertEqual(archive.read(name), metadata)
            self.assertEqual(set(archive.namelist()), {"PUBLIC_FILES.txt", "README.md", name})

    def test_rejects_listed_json_atlas_manifests(self):
        for name in ["atlas.json", "assets/atlas.json", "assets/ATLAS.JSON"]:
            with self.subTest(name=name):
                self.write(name, b'{"frames":{}}\n')
                self.manifest(["PUBLIC_FILES.txt", name])
                with self.assertRaisesRegex(DistributionError, "original asset manifest"):
                    audit(self.root)

    def test_rejects_listed_json_outside_reviewed_catalog_path(self):
        for name in ["metadata.json", "assets/skin.json", "assets/other_catalog.json",
                     "docs/config.json", "assets/SKIN_CATALOG.JSON"]:
            with self.subTest(name=name):
                self.write(name, b'{"name":"Synthetic metadata"}\n')
                self.manifest(["PUBLIC_FILES.txt", name])
                with self.assertRaisesRegex(DistributionError, "reviewed skin source catalog"):
                    audit(self.root)

    def test_rejects_traversal_duplicates_and_missing_files(self):
        for names in [
            ["PUBLIC_FILES.txt", "../README.md"],
            ["PUBLIC_FILES.txt", "README.md", "README.md"],
            ["PUBLIC_FILES.txt", "README.md", "readme.md"],
            ["PUBLIC_FILES.txt", "docs/NUL.md"],
            ["PUBLIC_FILES.txt", "docs/name:stream.md"],
            ["PUBLIC_FILES.txt", "missing.md"],
        ]:
            with self.subTest(names=names):
                self.manifest(names)
                with self.assertRaises(DistributionError):
                    audit(self.root)

    def test_rejects_symlink_files_and_directories(self):
        with tempfile.TemporaryDirectory() as outside:
            target = Path(outside) / "outside.md"
            target.write_text("Synthetic fixture\n")
            (self.root / "linked.md").symlink_to(target)
            self.manifest(["PUBLIC_FILES.txt", "linked.md"])
            with self.assertRaises(DistributionError):
                audit(self.root)
            (self.root / "linked").symlink_to(Path(outside), target_is_directory=True)
            self.manifest(["PUBLIC_FILES.txt", "linked/outside.md"])
            with self.assertRaises(DistributionError):
                audit(self.root)

    def test_rejects_binary_and_embedded_asset_data(self):
        cases = [
            bytes.fromhex("89504e470d0a1a0a"), b"\xff\xfe", b"binary\x00content",
            b"a" * (MAX_FILE_BYTES + 1),
            ("/" + "Users/example/private/source").encode(),
            ("0x" + "fff000ULL").encode(),
            (",".join(str(number) for number in range(80))).encode(),
            ("https://gist." + "githubusercontent.com/example/original.png").encode(),
        ]
        for data in cases:
            with self.subTest(prefix=repr(data[:25])):
                self.write("README.md", data)
                with self.assertRaises(DistributionError):
                    audit(self.root)

    def test_git_rejects_extra_staged_files(self):
        self.initialize_git()
        self.commit()
        self.write("extra.md", b"Unlisted synthetic source\n")
        self.git("add", "extra.md")
        with self.assertRaisesRegex(DistributionError, "outside"):
            audit(self.root, check_git=True)

    def test_git_rejects_staged_blob_even_when_working_file_is_clean(self):
        self.initialize_git()
        self.commit()
        self.write("README.md", bytes.fromhex("89504e470d0a1a0a"))
        self.git("add", "README.md")
        self.write("README.md", b"Clean working source\n")
        with self.assertRaisesRegex(DistributionError, "media"):
            audit(self.root, check_git=True)

    def test_git_rejects_deleted_historical_media(self):
        self.initialize_git()
        self.write("README.md", bytes.fromhex("89504e470d0a1a0a"))
        self.commit("Synthetic historical contamination")
        self.write("README.md", b"Clean current source\n")
        self.commit("Remove synthetic contamination")
        with self.assertRaisesRegex(DistributionError, "media"):
            audit(self.root, check_git=True)

    def test_git_rejects_deleted_historical_private_file(self):
        self.initialize_git()
        self.write("artifacts/secret.txt", b"Synthetic private fixture\n")
        self.commit()
        self.git("rm", "artifacts/secret.txt")
        self.commit("Remove synthetic private fixture")
        with self.assertRaisesRegex(DistributionError, "private"):
            audit(self.root, check_git=True)

    def test_git_accepts_clean_history(self):
        self.initialize_git()
        self.commit()
        self.write("README.md", b"Updated synthetic source\n")
        self.commit("Update synthetic source")
        self.assertEqual(set(audit(self.root, check_git=True)), {"PUBLIC_FILES.txt", "README.md"})

    def test_git_requires_complete_history(self):
        self.initialize_git()
        self.commit()
        head = self.git("rev-parse", "HEAD").stdout
        self.write(".git/shallow", head)
        with self.assertRaisesRegex(DistributionError, "complete clone"):
            audit(self.root, check_git=True)

    def test_git_audits_many_objects_without_stdin_batching(self):
        self.initialize_git()
        names = [f"docs/fixture-{number}.md" for number in range(40)]
        for number, name in enumerate(names):
            self.write(name, f"Synthetic source object {number}\n".encode())
        self.manifest(["PUBLIC_FILES.txt", "README.md", *names])
        self.commit()
        self.assertEqual(len(audit(self.root, check_git=True)), 42)

    def test_git_command_has_closed_stdin_and_reports_timeout(self):
        with mock.patch("check_distribution.subprocess.run", side_effect=subprocess.TimeoutExpired("git", 30)) as run:
            with self.assertRaisesRegex(DistributionError, "timed out"):
                git(self.root, "rev-parse", "HEAD")
        self.assertEqual(run.call_args.kwargs["stdin"], subprocess.DEVNULL)
        self.assertEqual(run.call_args.kwargs["timeout"], 30)

    def test_git_rejects_tagged_media_blob(self):
        self.initialize_git()
        self.commit()
        object_id = subprocess.run(
            ["git", "-C", str(self.root), "hash-object", "-w", "--stdin"],
            input=bytes.fromhex("89504e470d0a1a0a"), check=True, stdout=subprocess.PIPE,
        ).stdout.decode().strip()
        self.git("tag", "synthetic-media", object_id)
        with self.assertRaisesRegex(DistributionError, "media"):
            audit(self.root, check_git=True)

    def test_archive_cannot_overwrite_source(self):
        with self.assertRaises(DistributionError):
            package(self.root, self.root / "README.md")
        self.assertEqual((self.root / "README.md").read_bytes(), b"# Synthetic source fixture\n")


if __name__ == "__main__":
    unittest.main()
