import random
import subprocess
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BINARY = PROJECT_ROOT / "file-finder"


class FileFinderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        subprocess.run(
            ["make", "-C", str(PROJECT_ROOT)],
            check=True,
            capture_output=True,
            text=True,
        )

    @classmethod
    def tearDownClass(cls) -> None:
        subprocess.run(
            ["make", "-C", str(PROJECT_ROOT), "clean"],
            check=True,
            capture_output=True,
            text=True,
        )

    def run_finder(self, directory: Path, *patterns: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(BINARY), str(directory), *patterns],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_find_100_random_similar_text_files(self) -> None:
        random_generator = random.Random(42)

        with tempfile.TemporaryDirectory() as temporary_directory:
            search_directory = Path(temporary_directory)
            expected_matches = set()

            for index in range(100):
                random_suffix = "".join(
                    random_generator.choice("abcdefghijklmnopqrstuvwxyz0123456789")
                    for _ in range(6)
                )
                filename = f"report_{index:03d}_{random_suffix}.txt"
                (search_directory / filename).touch()
                expected_matches.add(filename)

            for index in range(20):
                (search_directory / f"unrelated_{index:03d}.log").touch()

            result = self.run_finder(search_directory, "report_")

            self.assertEqual(result.returncode, 0, result.stderr)

            output_lines = {
                line
                for line in result.stdout.splitlines()
                if line
                and not line.startswith("Search directory:")
                and not line.startswith("Substring count:")
            }
            self.assertEqual(output_lines, expected_matches)

    def test_empty_substring_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            result = self.run_finder(Path(temporary_directory), "")

            self.assertEqual(result.returncode, 1)
            self.assertIn("Usage:", result.stdout)


if __name__ == "__main__":
    unittest.main()
