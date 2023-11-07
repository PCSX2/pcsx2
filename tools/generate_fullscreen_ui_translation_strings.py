import os

START_IDENT = "// TRANSLATION-STRING-AREA-BEGIN"
END_IDENT = "// TRANSLATION-STRING-AREA-END"

src_file = os.path.join(os.path.dirname(__file__), "..", "pcsx2", "ImGui", "FullscreenUI.cpp")

with open(src_file, "r") as f:
    full_source = f.read()

strings = []
for token in ["FSUI_STR", "FSUI_CSTR", "FSUI_FSTR", "FSUI_NSTR", "FSUI_ICONSTR", "FSUI_ICONSTR_S"]:
    token_len = len(token)
    last_pos = 0
    while True:
        last_pos = full_source.find(token, last_pos)
        if last_pos < 0:
            break

        if last_pos >= 8 and full_source[last_pos - 8:last_pos] == "#define ":
            last_pos += len(token)
            continue

        if full_source[last_pos + token_len] == '(':
            start_pos = last_pos + token_len + 1
            end_pos = full_source.find("\")", start_pos)
            s = full_source[start_pos:end_pos+1]

            # remove "
            pos = s.find('"')
            new_s = ""
            while pos >= 0:
                if pos == 0 or s[pos - 1] != '\\':
                    epos = pos
                    while True:
                        epos = s.find('"', epos + 1)
                        assert epos > pos
                        if s[epos - 1] == '\\':
                            continue
                        else:
                            break

                    assert epos > pos
                    new_s += s[pos+1:epos]
                    cpos = s.find(',', epos + 1)
                    pos = s.find('"', epos + 1)
                    if cpos >= 0 and pos >= 0 and cpos < pos:
                        break
                else:
                    pos = s.find('"', pos + 1)
            assert len(new_s) > 0

            #assert (end_pos - start_pos) < 300
            #if (end_pos - start_pos) >= 300:
            #    print("WARNING: Long string")
            #    print(new_s)
            if new_s not in strings:
                strings.append(new_s)
        last_pos += len(token)

print(f"Found {len(strings)} unique strings.")

start = full_source.find(START_IDENT)
end = full_source.find(END_IDENT)
assert start >= 0 and end > start

new_area = ""
for string in list(strings):
    new_area += f"TRANSLATE_NOOP(\"FullscreenUI\", \"{string}\");\n"

full_source = full_source[:start+len(START_IDENT)+1] + new_area + full_source[end:]
with open(src_file, "w") as f:
    f.write(full_source)
