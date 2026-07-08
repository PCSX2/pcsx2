# A conversion script that generates proper GameDB files for use with ARMSX2
# Just drag and drop the GameIndex.yaml file from PCSX2 on to it to process it
# Requires the ruamel.yaml library to be installed to work
# Type pip install ruamel.yaml in the terminal to install it
import sys, os
from ruamel.yaml import YAML

def my_represent_none(self, data):
    return self.represent_scalar(u'tag:yaml.org,2002:null', u'null')

yaml = YAML()
yaml.indent(mapping=2, sequence=4, offset=2)
yaml.default_flow_style = False
yaml.preserve_quotes = True
yaml.allow_duplicate_keys = True
yaml.width = 512
yaml.representer.add_representer(type(None), my_represent_none)

key_list = ['clampModes', 'dynaPatches', 'gameFixes', 'gsHWFixes', 'memcardFilters', 'patches', 'roundModes', 'speedHacks']
key_order = ['name', 'name-sort', 'name-en', 'region', 'compat', 'clampModes', 'roundModes', 'gameFixes', 'speedHacks', 'gsHWFixes', 'patches', 'dynaPatches', 'memcardFilters']
clamp_list = ['eeClampMode', 'vuClampMode', 'vu0ClampMode', 'vu1ClampMode']
round_list = ['eeDivRoundMode', 'eeRoundMode', 'vuRoundMode', 'vu0RoundMode', 'vu1RoundMode']
gmfix_list = ['BlitInternalFPSHack', 'DMABusyHack', 'EETimingHack', 'FpuMulHack', 'GIFFIFOHack', 'GoemonTlbHack', 'IbitHack', 'OPHFlagHack', 'SkipMPEGHack', 'SoftwareRendererFMVHack', 'VIF1StallHack', 'VIFFIFOHack', 'VuAddSubHack', 'VUOverflowHack', 'FullVU0SyncHack', 'VUSyncHack', 'XGKickHack']
speed_list = ['mvuFlag', 'instantVU1', 'mtvu', 'eeCycleRate']
hwfix_list = ['autoFlush', 'cpuFramebufferConversion', 'readTCOnClose', 'disableDepthSupport', 'preloadFrameData', 'disablePartialInvalidation', 'partialTargetInvalidation', 'textureInsideRT', 'alignSprite', 'mergeSprite', 'forceEvenSpritePosition', 'bilinearUpscale', 'nativePaletteDraw', 'estimateTextureRegion', 'PCRTCOffsets', 'PCRTCOverscan', 'mipmap', 'trilinearFiltering', 'skipDrawStart', 'skipDrawEnd', 'halfBottomOverride', 'halfPixelOffset', 'nativeScaling', 'roundSprite', 'texturePreloading', 'deinterlace', 'cpuSpriteRenderBW', 'cpuSpriteRenderLevel', 'cpuCLUTRender', 'gpuTargetCLUT', 'gpuPaletteConversion', 'minimumBlendingLevel', 'maximumBlendingLevel', 'getSkipCount', 'beforeDraw', 'moveHandler']
ignore_keys = []
ignore_list = ['GSC_IRem', 'GSC_SandGrainGames', 'GSC_Turok', 'recommendedBlendingLevel']
gamefix_dict = {'SLES-54822': ['SoftwareRendererFMVHack'], 'SLUS-21327': ['SoftwareRendererFMVHack'], 'SLUS-21564': ['SoftwareRendererFMVHack'], 'SLES-51252': ['SoftwareRendererFMVHack'], 'SLPM-65212': ['SoftwareRendererFMVHack'], 'SLPM-67005': ['SoftwareRendererFMVHack'], 'SLPM-67546': ['SoftwareRendererFMVHack'], 'SLPS-29003': ['SoftwareRendererFMVHack'], 'SLPS-29004': ['SoftwareRendererFMVHack'], 'SLUS-20578': ['SoftwareRendererFMVHack']}
hwfkey_dict = {}
replace_dict = {'nativeScaling: 3': 'nativeScaling: 1', 'nativeScaling: 4': 'nativeScaling: 2', 'PlayStation2': 'PlayStation 2'}
speedfix_dict = {'SLPM-60149': ['mvuFlag', 0], 'SLPS-25052': ['mvuFlag', 0], 'SLPS-73205': ['mvuFlag', 0], 'SLPS-73410': ['mvuFlag', 0], 'SLUS-20152': ['mvuFlag', 0]}

def sort_keys(my_dict):
    sorted_data = {}
    for key, value in my_dict.items():
        if isinstance(value, dict):
            sorted_nested = {}
            for nested_key in key_order:
                if nested_key in value: sorted_nested[nested_key] = value[nested_key]
            sorted_data[key] = sorted_nested
        else: sorted_data[key] = value
    return sorted_data

def process_db(file_name, clean_name):
    print('Processing ' + os.path.basename(file_name) + '...')
    if os.path.isfile('GameIndex[temp].yaml'): os.remove('GameIndex[temp].yaml')
    if os.path.isfile(clean_name) and clean_name == 'GameIndex[converted].yaml': os.remove('GameIndex[converted].yaml')
    with open(file_name, encoding='utf8') as newfile, open('GameIndex[temp].yaml', 'w', encoding='utf8') as tempfile:
        for line in newfile:
            if any(k := key for key in replace_dict if key in line): line = line.replace(k, replace_dict[k])
            if not any(ignore_word in line for ignore_word in ignore_list): tempfile.write(line)
    os.rename('GameIndex[temp].yaml', clean_name)

def process_dict(my_dict, new_dict):
    req_sort = False
    my_dict = {k: v for k, v in my_dict.items() if any(rk in v for rk in key_list) or k in hwfkey_dict or k in gamefix_dict or k in speedfix_dict}
    for key, value in my_dict.items():
        for nested_key in ['name', 'name-sort', 'name-en', 'region', 'compat']:
            if nested_key in value and key in new_dict:
                try:
                    if not my_dict[key][nested_key] == new_dict[key][nested_key]:
                        my_dict[key][nested_key] = new_dict[key][nested_key] 
                except KeyError: continue
    for key, value in new_dict.items():
        if key in ignore_keys: continue
        for nested_key in key_list:
            if nested_key in value and key in my_dict:
                try:
                    if my_dict[key][nested_key]: continue
                except KeyError:
                    if not req_sort: req_sort = True
                    my_dict[key][nested_key] = new_dict[key][nested_key]
        for nested_key in ['name-sort', 'name-en', 'compat']:
            if nested_key in value and key in my_dict: my_dict[key][nested_key] = new_dict[key][nested_key]
        if 'clampModes' in value and key in my_dict:
            for nested_key in ['vu0ClampMode', 'vu1ClampMode']:
                if nested_key in value['clampModes']:
                    try:
                        if my_dict[key]['clampModes']['vuClampMode']:
                            del my_dict[key]['clampModes']['vuClampMode']
                            my_dict[key]['clampModes'][nested_key] = new_dict[key]['clampModes'][nested_key]
                    except KeyError: continue
            for nested_key in clamp_list:
                if nested_key in value['clampModes']:
                    try:
                        if my_dict[key]['clampModes'][nested_key]: continue
                    except KeyError:
                        if 'vuClampMode' in my_dict[key]['clampModes'] and nested_key != 'vuClampMode':
                            del my_dict[key]['clampModes']['vuClampMode']
                        my_dict[key]['clampModes'][nested_key] = new_dict[key]['clampModes'][nested_key]
        if 'roundModes' in value and key in my_dict:
            for nested_key in ['vu0RoundMode', 'vu1RoundMode']:
                if nested_key in value['roundModes']:
                    try:
                        if my_dict[key]['roundModes']['vuRoundMode']:
                            del my_dict[key]['roundModes']['vuRoundMode']
                            my_dict[key]['roundModes'][nested_key] = new_dict[key]['roundModes'][nested_key]
                    except KeyError: continue
            for nested_key in round_list:
                if nested_key in value['roundModes']:
                    try:
                        if my_dict[key]['roundModes'][nested_key]: continue
                    except KeyError:
                        if 'vuRoundMode' in my_dict[key]['roundModes'] and nested_key != 'vuRoundMode':
                            del my_dict[key]['roundModes']['vuRoundMode']
                        my_dict[key]['roundModes'][nested_key] = new_dict[key]['roundModes'][nested_key]
        if 'gameFixes' in value and key in my_dict:
            for nested_value in gmfix_list:
                if nested_value in value['gameFixes']:
                    if nested_value in my_dict[key]['gameFixes']: continue
                    my_dict[key]['gameFixes'].append(nested_value)
        if 'speedHacks' in value and key in my_dict:
            for nested_key in speed_list:
                if nested_key in value['speedHacks']:
                    my_dict[key]['speedHacks'][nested_key] = new_dict[key]['speedHacks'][nested_key]
        if 'gsHWFixes' in value and key in my_dict:
            for nested_key in hwfix_list:
                if nested_key in value['gsHWFixes']:
                    my_dict[key]['gsHWFixes'][nested_key] = new_dict[key]['gsHWFixes'][nested_key]
        if key in gamefix_dict and key in my_dict:
            for i in range(len(gamefix_dict[key])):
                if 'gameFixes' in my_dict[key]: 
                    if gamefix_dict[key][i] not in my_dict[key]['gameFixes']: 
                        my_dict[key]['gameFixes'].append(gamefix_dict[key][i])
                else: my_dict[key]['gameFixes'] = [gamefix_dict[key][i]]
        if key in speedfix_dict and key in my_dict:
            if 'speedHacks' not in my_dict[key]: my_dict[key]['speedHacks'] = {}
            for i in range(0, len(speedfix_dict[key]), 2): 
                my_dict[key]['speedHacks'][speedfix_dict[key][i]] = speedfix_dict[key][i + 1]
        if key in hwfkey_dict and key in my_dict:
            if 'gsHWFixes' not in my_dict[key]: my_dict[key]['gsHWFixes'] = {}
            for i in range(0, len(hwfkey_dict[key]), 2): 
                my_dict[key]['gsHWFixes'][hwfkey_dict[key][i]] = hwfkey_dict[key][i + 1]
    if req_sort: my_dict.update(sort_keys(my_dict))
    return my_dict

def fix_db(file_name):
    print('Removing invalid keys from ' + file_name + '...')
    with open(file_name, encoding='utf8') as newfile, open('GameIndex[temp].yaml', 'w', encoding='utf8') as tempfile:
        data = yaml.load(newfile)
        yaml.dump(data, tempfile)
    with open('GameIndex[temp].yaml', encoding='utf8') as tempfile, open(file_name, 'w', encoding='utf8') as newfile:
        for line in tempfile:
            if '{' in line: line = line.replace('{', '{ ')
            if '}' in line: line = line.replace('}', ' }')
            if ': null' not in line: newfile.write(line)
    os.remove('GameIndex[temp].yaml')

if len(sys.argv) > 1 and os.path.isfile(sys.argv[1]): gamedb_file = sys.argv[1]
else: 
    print('Usage: python gamedb-convert.py GameIndex.yaml')
    sys.exit()

process_db(gamedb_file, 'GameIndex[converted].yaml')
print('Creating GameIndex[converted].yaml...')
fix_db('GameIndex[converted].yaml')
if os.path.isfile('GameIndex[temp2].yaml'): os.remove('GameIndex[temp2].yaml')
with open('GameIndex[converted].yaml', encoding='utf8') as base, open('files/GameIndex[original].yaml', encoding='utf8') as og, open('files/GameIndex[override].yaml', encoding='utf8') as diff, open('GameIndex[merged].yaml', 'w', encoding='utf8') as merged:
    print('Loading GameDB entries to merge...')
    base_db = yaml.load(base)
    og_db = yaml.load(og)
    diff_db = yaml.load(diff)
    print('Processing older GameDB prior to merging...')
    og_db = process_dict(og_db, base_db)
    print('Merging GameDB entries...')
    base_db.update(og_db)
    base_db.update(diff_db)
    print('Creating GameIndex[merged].yaml file...')
    yaml.dump(base_db, merged)
process_db('GameIndex[merged].yaml', 'GameIndex[temp2].yaml')
if os.path.isfile('GameIndex[merged].yaml'): os.remove('GameIndex[merged].yaml')
os.rename('GameIndex[temp2].yaml', 'GameIndex[merged].yaml')
fix_db('GameIndex[merged].yaml')
print('All Done!')