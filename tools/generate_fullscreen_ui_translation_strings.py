#!/usr/bin/env python3

import os

START_IDENT = "// TRANSLATION-STRING-AREA-BEGIN"
END_IDENT = "// TRANSLATION-STRING-AREA-END"

src_files = [
    os.path.join(os.path.dirname(__file__), "..", "pcsx2", "ImGui", "FullscreenUI.cpp"),
    os.path.join(os.path.dirname(__file__), "..", "pcsx2", "ImGui", "FullscreenUI_Settings.cpp"),
]

def extract_strings_from_source(source_content):
    """Extract FSUI translation strings from source content."""
    strings = []
    for token in ["FSUI_STR", "FSUI_CSTR", "FSUI_FSTR", "FSUI_NSTR", "FSUI_VSTR", "FSUI_ICONSTR", "FSUI_ICONSTR_S"]:
        token_len = len(token)
        last_pos = 0
        while True:
            last_pos = source_content.find(token, last_pos)
            if last_pos < 0:
                break

            if last_pos >= 8 and source_content[last_pos - 8:last_pos] == "#define ":
                last_pos += len(token)
                continue

            if source_content[last_pos + token_len] == '(':
                start_pos = last_pos + token_len + 1
                end_pos = source_content.find(")", start_pos)
                s = source_content[start_pos:end_pos]

                # Split into string arguments, removing "
                string_args = [""]
                arg = 0;
                cpos = s.find(',')
                pos = s.find('"')
                while pos >= 0 or cpos >= 0:
                    assert pos == 0 or s[pos - 1] != '\\'
                    if cpos == -1 or pos < cpos:
                        epos = pos
                        while True:
                            epos = s.find('"', epos + 1)
                            # found ')' in string, extend s to next ')'
                            if epos == -1:
                                end_pos = source_content.find(")", end_pos + 1)
                                s = source_content[start_pos:end_pos]
                                epos = pos
                                continue

                            if s[epos - 1] == '\\':
                                continue
                            else:
                                break

                        assert epos > pos
                        string_args[arg] += s[pos+1:epos]
                        cpos = s.find(',', epos + 1)
                        pos = s.find('"', epos + 1)
                    else:
                        arg += 1
                        string_args.append("")
                        cpos = s.find(',', cpos + 1)

                print(string_args)

                # FSUI_ICONSTR and FSUI_ICONSTR_S need to translate the only the second argument
                # other defines take only a single argument
                if len(string_args) >= 2:
                    new_s = string_args[1]
                else:
                    new_s = string_args[0]

                assert len(new_s) > 0

                if new_s not in strings:
                    strings.append(new_s)
            last_pos += len(token)
    return strings

def process_file(src_file):
    """Process a single source file extract strings and update its translation area."""
    print(f"\nProcessing: {src_file}")
    
    with open(src_file, "r") as f:
        source = f.read()

    start = source.find(START_IDENT)
    end = source.find(END_IDENT)
    
    if start < 0 or end <= start:
        print(f"  Warning: No translation string area found in {src_file}")
        return 0
    
    source_without_area = source[:start] + source[end + len(END_IDENT):]
    strings = extract_strings_from_source(source_without_area)
    
    print(f"  Found {len(strings)} unique strings.")
    
    new_area = ""
    for string in strings:
        new_area += f"TRANSLATE_NOOP(\"FullscreenUI\", \"{string}\");\n"
    
    new_source = source[:start + len(START_IDENT) + 1] + new_area + source[end:]
    with open(src_file, "w") as f:
        f.write(new_source)
    
    return len(strings)

total_strings = 0
for src_file in src_files:
    total_strings += process_file(src_file)

print(f"\nTotal: {total_strings} unique strings across all files.")
