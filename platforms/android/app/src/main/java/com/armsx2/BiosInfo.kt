package com.armsx2

/**
 * BIOS info as parsed by emucore's IsBIOSFromFd. Region values are the same
 * values [BiosTools.cpp:138] returns from the ROMVER fifth-byte switch:
 *
 * - 0  Japan
 * - 1  USA
 * - 2  Europe
 * - 4  Asia (H)
 * - 6  China
 * - 8  T10K / COH-H (debug board)
 * - 9  Test
 * - 10 Free
 *
 * `version` packs major in the high byte and minor in the low byte
 * (e.g. v2.30 → 0x0230).
 *
 * Constructed from native code via JNI; keep the constructor signature
 * (I,I,Ljava/lang/String;,Ljava/lang/String;) in sync with native-lib.cpp.
 */
class BiosInfo(
    @JvmField val version: Int,
    @JvmField val region: Int,
    @JvmField val description: String,
    @JvmField val zone: String,
) {
    /** "v2.30" form derived from the packed version int. */
    val versionString: String get() = "v%d.%02d".format((version shr 8) and 0xFF, version and 0xFF)

    /** Unicode flag emoji for the region, or a globe for unknown regions. */
    val regionFlag: String get() = when (region) {
        0 -> "🇯🇵"   // 🇯🇵 Japan
        1 -> "🇺🇸"   // 🇺🇸 USA
        2 -> "🇪🇺"   // 🇪🇺 Europe
        4 -> "🇭🇰"   // 🇭🇰 Asia (Hong Kong stand-in — there's no pan-Asia flag)
        6 -> "🇨🇳"   // 🇨🇳 China
        8 -> "🔧"                // 🔧 T10K / COH-H devkit
        9 -> "🧪"                // 🧪 Test
        10 -> "🏳️"        // 🏳️ Free / region-free
        else -> "🌐"            // 🌐 unknown
    }
}
