import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class TrueCrimeGameDBTests(unittest.TestCase):
    SERIALS = (
        "SLES-53616",
        "SLES-53617",
        "SLES-53618",
        "SLPM-66473",
        "SLPM-74243",
        "SLUS-21106",
    )

    def test_all_new_york_city_entries_disable_vu_flag_hack(self):
        game_index = (
            ROOT / "app/src/main/assets/resources/GameIndex.yaml"
        ).read_text(encoding="utf-8")

        for serial in self.SERIALS:
            with self.subTest(serial=serial):
                match = re.search(
                    rf"(?ms)^{re.escape(serial)}:\n(.*?)(?=^[A-Z0-9-]+:\n|\Z)",
                    game_index,
                )
                self.assertIsNotNone(match, f"Missing GameDB entry for {serial}")
                self.assertRegex(
                    match.group(1),
                    r"(?m)^  speedHacks:\n(?:    .*\n)*?"
                    r"    mvuFlag: 0(?:\s+#.*)?$",
                )


if __name__ == "__main__":
    unittest.main()
