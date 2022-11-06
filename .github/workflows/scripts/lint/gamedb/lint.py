import os
import yaml

# Assumes this is ran from the root of the repository
file_path = os.environ['GAMEDB_PATH']

# These settings have to be manually kept in sync with the emulator code unfortunately.
# up to date validation should ALWAYS be provided via the application!
allowed_game_options = [
    "name",
    "region",
    "compat",
    "roundModes",
    "clampModes",
    "gameFixes",
    "gsHWFixes",
    "speedHacks",
    "memcardFilters",
    "patches",
]
allowed_round_modes = ["eeRoundMode", "vuRoundMode"]
allowed_clamp_modes = ["eeClampMode", "vuClampMode"]
allowed_game_fixes = [
    "FpuMulHack",
    "FpuNegDivHack",
    "GoemonTlbHack",
    "SoftwareRendererFMVHack",
    "SkipMPEGHack",
    "OPHFlagHack",
    "EETimingHack",
    "InstantDMAHack",
    "DMABusyHack",
    "GIFFIFOHack",
    "VIFFIFOHack",
    "VIF1StallHack",
    "VuAddSubHack",
    "IbitHack",
    "VUSyncHack",
    "VUOverflowHack",
    "XGKickHack",
    "BlitInternalFPSHack",
]
allowed_gs_hw_fixes = [
    "autoFlush",
    "conservativeFramebuffer",
    "cpuFramebufferConversion",
    "disableDepthSupport",
    "wrapGSMem",
    "preloadFrameData",
    "disablePartialInvalidation",
    "textureInsideRT",
    "alignSprite",
    "mergeSprite",
    "wildArmsHack",
    "pointListPalette",
    "mipmap",
    "trilinearFiltering",
    "skipDrawStart",
    "skipDrawEnd",
    "halfBottomOverride",
    "halfPixelOffset",
    "roundSprite",
    "texturePreloading",
    "deinterlace",
    "cpuSpriteRenderBW",
    "cpuCLUTRender",
    "gpuPaletteConversion",
]
gs_hw_fix_ranges = {
    "mipmap": (0, 2),
    "trilinearFiltering": (0, 2),
    "skipDrawStart": (0, 100000),
    "skipDrawEnd": (0, 100000),
    "halfPixelOffset": (0, 3),
    "roundSprite": (0, 2),
    "deinterlace": (0, 7),
    "cpuSpriteRenderBW": (1, 10),
    "cpuCLUTRender": (1, 2),
    "gpuPaletteConversion": (0, 2),
}
allowed_speed_hacks = ["mvuFlagSpeedHack", "InstantVU1SpeedHack", "MTVUSpeedHack"]
# Patches are allowed to have a 'default' key or a crc-32 key, followed by
allowed_patch_options = ["author", "content"]

issue_list = []


def is_hex_number(string):
    try:
        int(string, 16)
        return True
    except Exception:
        return False


def validate_string_option(serial, key, value):
    if not isinstance(value, str):
        issue_list.append("[{}]: '{}' must be a string".format(serial, key))


def validate_int_option(serial, key, value, low, high):
    if not isinstance(value, int) or (value < low or value > high):
        issue_list.append(
            "[{}]: '{}' must be an int and between {}-{} (inclusive)".format(
                serial, key, low, high
            )
        )


def validate_list_of_strings(serial, key, value):
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        issue_list.append("[{}]: '{}' must be a list of strings".format(serial, key))


def validate_valid_options(serial, key, value, allowed_values):
    if value not in allowed_values:
        issue_list.append("[{}]: Invalid '{}' option [{}]".format(serial, key, value))


def validate_clamp_round_modes(serial, key, value, allowed_values):
    if not isinstance(value, dict):
        issue_list.append("[{}]: '{}' must be a valid object".format(serial, key))
        return
    for mode_key, mode_value in value.items():
        validate_valid_options(serial, key, mode_key, allowed_values)
        validate_int_option(serial, key, mode_value, 0, 3)


def validate_game_fixes(serial, key, value):
    if not isinstance(value, list):
        issue_list.append(
            "[{}]: 'gameFixes' must be a list of valid gameFixes".format(serial)
        )
        return
    for gamefix in value:
        validate_valid_options(serial, key, gamefix, allowed_game_fixes)


def validate_gs_hw_fix_value(serial, key, value):
    low, high = 0, 1
    if key in gs_hw_fix_ranges:
        low, high = gs_hw_fix_ranges[key]
    validate_int_option(serial, key, value, low, high)


def validate_gs_hw_fixes(serial, key, value):
    if not isinstance(value, dict):
        issue_list.append("[{}]: 'gsHWFixes' must be a valid object".format(serial))
        return
    for fix, fix_value in value.items():
        validate_valid_options(serial, key, fix, allowed_gs_hw_fixes)
        validate_gs_hw_fix_value(serial, fix, fix_value)

    # skipdraw range must have end >= start
    skip_draw_start = value["skipDrawStart"] if "skipDrawStart" in value else 0
    skip_draw_end = value["skipDrawEnd"] if "skipDrawEnd" in value else 0
    if isinstance(skip_draw_start, int) and isinstance(skip_draw_end, int) and skip_draw_end < skip_draw_start:
        issue_list.append("[{}]: skipDrawStart({}) must be greater or equal to skipDrawEnd({})".format(
            serial, skip_draw_start, skip_draw_end))


def validate_speed_hacks(serial, key, value):
    if not isinstance(value, dict):
        issue_list.append("[{}]: 'speedHacks' must be a valid object".format(serial))
        return
    for speedhack, speedhack_value in value.items():
        validate_valid_options(serial, key, speedhack, allowed_speed_hacks)
        validate_int_option(serial, speedhack, speedhack_value, 0, 1)


def validate_patches(serial, key, value):
    if not isinstance(value, dict):
        issue_list.append(
            "[{}]: 'patches' must be valid mapping of CRC32 -> Patch Objects".format(
                serial
            )
        )
        return
    for crc, patch in value.items():
        if crc != "default" and not is_hex_number(str(crc)):
            issue_list.append(
                "[{}]: Patches must either be key'd with 'default' or a valid CRC-32 Hex String".format(
                    serial
                )
            )
            continue
        for patch_option, option_value in patch.items():
            validate_valid_options(serial, key, patch_option, allowed_patch_options)
            if patch_option == "author":
                validate_string_option(serial, patch_option, option_value)
            if patch_option == "content":
                validate_string_option(serial, patch_option, option_value)


# pylint:disable=unnecessary-lambda
option_validation_handlers = {
    "name": (lambda serial, key, value: validate_string_option(serial, key, value)),
    "region": (lambda serial, key, value: validate_string_option(serial, key, value)),
    "compat": (
        lambda serial, key, value: validate_int_option(serial, key, value, 0, 6)
    ),
    "roundModes": (
        lambda serial, key, value: validate_clamp_round_modes(
            serial, key, value, allowed_round_modes
        )
    ),
    "clampModes": (
        lambda serial, key, value: validate_clamp_round_modes(
            serial, key, value, allowed_clamp_modes
        )
    ),
    "gameFixes": (lambda serial, key, value: validate_game_fixes(serial, key, value)),
    "gsHWFixes": (lambda serial, key, value: validate_gs_hw_fixes(serial, key, value)),
    "speedHacks": (lambda serial, key, value: validate_speed_hacks(serial, key, value)),
    "memcardFilters": (
        lambda serial, key, value: validate_list_of_strings(serial, key, value)
    ),
    "patches": (lambda serial, key, value: validate_patches(serial, key, value)),
}

class UniqueKeyLoader(yaml.FullLoader):
    def construct_mapping(self, node, deep=False):
        mapping = set()
        for key_node, _ in node.value:
            key = self.construct_object(key_node, deep=deep)
            if key in mapping:
                raise ValueError(f"Duplicate {key!r} key found in YAML.")
            mapping.add(key)
        return super().construct_mapping(node, deep)

print("Opening {}...".format(file_path))
with open(file_path, encoding="utf-8") as f:
    try:
        print("Attempting to parse GameDB file...")
        gamedb = yaml.load(f, Loader=UniqueKeyLoader)
    except Exception as err:
        print(err)
        print(
            "Unable to parse GameDB. Exiting, verify that the file indeed is valid YAML."
        )
        exit(1)

    print("File loaded successfully, validating schema...")
    progress_counter = 0
    for serial, game_options in gamedb.items():
        progress_counter = progress_counter + 1
        if progress_counter % 500 == 0 or progress_counter >= len(gamedb.items()):
            print(
                "[{}/{}] Processing GameDB Entries...".format(
                    progress_counter, len(gamedb.items())
                )
            )

        # Check for required values
        if not "name" in game_options.keys():
            issue_list.append("[{}]: 'name' is a required value".format(serial))
        if not "region" in game_options.keys():
            issue_list.append("[{}]: 'region' is a required value".format(serial))

        # Check the options
        for key, value in game_options.items():
            if key not in allowed_game_options:
                issue_list.append("[{}]: Invalid option [{}]".format(serial, key))
                continue

            if key in option_validation_handlers:
                option_validation_handlers[key](serial, key, value)

if len(issue_list) > 0:
    print("Issues found during validation:")
    print(*issue_list, sep="\n")
    exit(1)
else:
    print("GameDB Validated Successfully!")
    exit(0)
