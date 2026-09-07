#!/usr/bin/env python3
from __future__ import annotations
import hashlib, json, pathlib, struct, subprocess, sys, tempfile, zipfile

EXPECTED_FILENAME='libGoogleUI_V1.0.0.levipack'
ROOT='google_ui/'
ENTRY='libgoogle_ui.so'
EXPECTED_MANIFEST={
    'type':'preload-native','name':'GoogleUI','author':'xiomi','version':'1.0.0',
    'entry':ENTRY,'icon':'resources/google_button.png','minecraft_versions':['26.45.*']}
EXPECTED_CONFIG={'default_enabled':True,'auto_resolution':True,'ad_blocking':True,'button_url':'https://www.google.com'}
EXPECTED_MC='444e77434bdd3789a0d90978d06336a99831e78e52955e528258cc375dfa0557'
EXPECTED_PRE='8d628e4251498a867f9058c045068207616af170dde7b0962528fd4d54e47fed'

def sha256(b): return hashlib.sha256(b).hexdigest()

def readelf(path,*args): return subprocess.run(['readelf',*args,str(path)],check=True,text=True,capture_output=True).stdout

def verify_elf(data):
    if data[:4]!=b'\x7fELF' or data[4:6]!=b'\x02\x01': raise ValueError('entry must be ELF64 LE')
    e_type,e_machine=struct.unpack_from('<HH',data,16)
    if (e_type,e_machine)!=(3,183): raise ValueError('entry must be AArch64 DYN')
    with tempfile.TemporaryDirectory(prefix='googleui-elf-') as td:
        p=pathlib.Path(td)/ENTRY; p.write_bytes(data)
        dyn=readelf(p,'-d'); syms=readelf(p,'-Ws'); notes=readelf(p,'-n')
    needed=[x for x in dyn.splitlines() if '(NEEDED)' in x]
    if needed!=[x for x in needed if 'libpreloader.so' in x]: raise ValueError(f'unexpected DT_NEEDED: {needed}')
    if 'PLGetModRegistration' not in syms: raise ValueError('PLGetModRegistration missing')
    for marker in (b'BedrockTools',b'bedrocktools',b'ChunkBase',b'chunkbase'):
        if marker in data: raise ValueError(f'unwanted marker remains: {marker!r}')
    for marker in (b'https://www.google.com',b'nativeGetExternalModsInfo',b'nativeGetDrawCommands',b'nativeGetDrawCommandsRevision',b'nativeGetExternalButtonIconBytes',b'dex\n035\0'):
        if marker not in data: raise ValueError(f'required ELF marker missing: {marker!r}')
    loads=[]
    with tempfile.TemporaryDirectory(prefix='googleui-elf2-') as td:
        p=pathlib.Path(td)/ENTRY; p.write_bytes(data)
        hdr=readelf(p,'-l')
    for line in hdr.splitlines():
        if 'LOAD' in line and '0x' in line: pass
    return {'size':len(data),'sha256':sha256(data),'build_id':next((x.split('Build ID:',1)[1].strip() for x in notes.splitlines() if 'Build ID:' in x), '')}

def main():
    if len(sys.argv)!=2: raise SystemExit(f'usage: {pathlib.Path(sys.argv[0]).name} <package>')
    package=pathlib.Path(sys.argv[1]).resolve()
    if package.name!=EXPECTED_FILENAME: raise ValueError(f'expected {EXPECTED_FILENAME}')
    with zipfile.ZipFile(package) as z:
        names=z.namelist()
        if z.testzip(): raise ValueError('ZIP CRC failure')
        if any(not n.startswith(ROOT) or pathlib.PurePosixPath(n).is_absolute() or '..' in pathlib.PurePosixPath(n).parts for n in names):
            raise ValueError('unsafe package member')
        required={ROOT+'manifest.json',ROOT+ENTRY,ROOT+'config/config.json',ROOT+'config/config.schema.json',ROOT+'resources/google_button.png',ROOT+'resources/compatibility.json',ROOT+'resources/adblock/hosts.txt',ROOT+'resources/adblock/url_tokens.txt'}
        missing=required-set(names)
        if missing: raise ValueError(f'missing: {sorted(missing)}')
        manifest=json.loads(z.read(ROOT+'manifest.json')); config=json.loads(z.read(ROOT+'config/config.json')); schema=json.loads(z.read(ROOT+'config/config.schema.json')); compat=json.loads(z.read(ROOT+'resources/compatibility.json'))
        if manifest!=EXPECTED_MANIFEST: raise ValueError(f'manifest mismatch: {manifest}')
        if config!=EXPECTED_CONFIG: raise ValueError(f'config mismatch: {config}')
        if set(schema.get('properties',{}))!=set(EXPECTED_CONFIG): raise ValueError('config schema mismatch')
        if compat['minecraft']['sha256']!=EXPECTED_MC: raise ValueError('Minecraft fingerprint mismatch')
        if compat['levilauncher']['sha256']!=EXPECTED_PRE: raise ValueError('preloader fingerprint mismatch')
        elf=verify_elf(z.read(ROOT+ENTRY))
        out={'status':'PASS','package_sha256':sha256(package.read_bytes()),'manifest':manifest,'config':config,'elf':elf}
    print(json.dumps(out,indent=2))

if __name__=='__main__': main()
