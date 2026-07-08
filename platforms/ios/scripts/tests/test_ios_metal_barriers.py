import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class IOSMetalBarrierTests(unittest.TestCase):
    def test_ios_does_not_advertise_unsupported_texture_barriers(self):
        implementation = (
            ROOT / "app/src/main/cpp/pcsx2/GS/Renderers/Metal/GSDeviceMTL.mm"
        ).read_text(encoding="utf-8")

        self.assertRegex(
            implementation,
            r"(?m)^#if TARGET_OS_IPHONE\n"
            r"\s*m_features\.texture_barrier = false;\n"
            r"#else\n"
            r"\s*m_features\.texture_barrier = true;\n"
            r"#endif$",
        )
        self.assertNotIn("[enc textureBarrier];", implementation)


if __name__ == "__main__":
    unittest.main()
