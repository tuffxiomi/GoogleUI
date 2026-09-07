#!/usr/bin/env python3
from pathlib import Path
import hashlib, json, sys, zipfile
ROOT = Path(__file__).resolve().parents[1]
checks = []
def check(name, ok, detail=''):
    checks.append((name, bool(ok), detail))
manifest = json.loads((ROOT/'manifest.json').read_text())
config = json.loads((ROOT/'config/config.json').read_text())
schema = json.loads((ROOT/'config/config.schema.json').read_text())
compat = json.loads((ROOT/'compatibility/target.json').read_text())
source_files = [p for p in ROOT.rglob('*') if p.is_file() and p.suffix in {'.cpp','.hpp','.h','.json','.js','.in','.yml','.yaml'}]
alltext = '\n'.join(p.read_text(errors='ignore') for p in source_files)
check('GoogleUI manifest', manifest.get('name') == 'GoogleUI' and manifest.get('entry') == 'libgoogle_ui.so')
check('26.45 selector', manifest.get('minecraft_versions') == ['26.45.*'])
check('Google default URL', config.get('button_url') == 'https://www.google.com')
check('schema matches config keys', set(schema.get('properties', {})) == set(config))
with zipfile.ZipFile('/mnt/data/libminecraftpe.zip') as z:
    actual_mc = hashlib.sha256(z.read('libminecraftpe.so')).hexdigest()
expected_mc = compat.get('minecraft', {}).get('sha256', '')
check('Minecraft fingerprint present', expected_mc == actual_mc, f'expected {expected_mc}, actual {actual_mc}')
check('no legacy source references', 'chunkbase' not in alltext.lower() and 'bedrocktools' not in alltext.lower())
check('no dependency directories', not (ROOT/'include/bedrocktools').exists() and not (ROOT/'src/modules').exists())
state = (ROOT/'include/RuntimeState.hpp').read_text()
check('hardcoded Google URL', 'https://www.google.com' in state)
bridge = (ROOT/'src/ModMenuBridgeHooks.cpp').read_text()
check('bulk bridge hooks present', all(s in bridge for s in ('nativeGetExternalModsInfo','nativeSetExternalModConfig','nativeGetDrawCommands','nativeGetDrawCommandsRevision')))
check('external button bridge present', all(s in bridge for s in ('nativeGetExternalButtonCount','nativeGetExternalButtonInfo','nativeGetExternalButtonIconBytes','nativeDispatchExternalButtonEvent')))
check('Google button asset exists', (ROOT/'resources/google_button.png').is_file())
check('adblock data regenerated', (ROOT/'src/AdBlockData.cpp').is_file() and 'google_ui::adblock_data' in (ROOT/'src/AdBlockData.cpp').read_text())
passed = sum(ok for _, ok, _ in checks)
for n, ok, d in checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {n}" + (f' — {d}' if d and not ok else ''))
print(f'Result: {passed}/{len(checks)} PASS')
if passed != len(checks):
    sys.exit(1)
