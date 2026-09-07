#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
TARGET = json.loads((ROOT / "compatibility/target.json").read_text())
REQUIRED_GLOSS = {
    "GlossClose", "GlossHook", "GlossHookDelete", "GlossInit", "GlossOpen",
    "GlossSymbol", "GlossSymbolEx"
}
JNI_PREFIX = "Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_"
EXPECTED_LAYOUTS = {
    "ModRegistration": {"size": 40, "members": {"instance": 0, "load": 8, "enable": 16, "disable": 24, "unload": 32}},
    "ModContext": {"size": 256, "members": {"mJavaVm": 0, "mInfo": 8, "mLogger": 248}},
    "ModInfo": {"size": 240, "members": {"modRootPath": 216}},
}


def run(*args: str) -> str:
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def build_id(path: pathlib.Path) -> str:
    notes = run("readelf", "-n", str(path))
    match = re.search(r"Build ID:\s*([0-9a-fA-F]+)", notes)
    if not match:
        raise ValueError("GNU build ID is missing")
    return match.group(1).lower()


def symbols(path: pathlib.Path) -> dict[str, tuple[int, int]]:
    result: dict[str, tuple[int, int]] = {}
    for line in run("readelf", "-Ws", str(path)).splitlines():
        fields = line.split()
        if len(fields) < 8 or not fields[0].endswith(":"):
            continue
        try:
            value = int(fields[1], 16)
            size = int(fields[2], 10)
        except ValueError:
            continue
        name = fields[7]
        if name not in result or fields[4] == "GLOBAL":
            result[name] = (value, size)
    return result


def load_segments(data: bytes) -> list[tuple[int, int, int, int]]:
    if data[:6] != b"\x7fELF\x02\x01":
        raise ValueError("not ELF64 little-endian")
    e_type, e_machine = struct.unpack_from("<HH", data, 16)
    if (e_type, e_machine) != (3, 183):
        raise ValueError(f"unexpected ELF identity {(e_type, e_machine)}")
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phentsize = struct.unpack_from("<H", data, 54)[0]
    phnum = struct.unpack_from("<H", data, 56)[0]
    result = []
    for index in range(phnum):
        offset = phoff + index * phentsize
        if struct.unpack_from("<I", data, offset)[0] != 1:
            continue
        result.append((
            struct.unpack_from("<Q", data, offset + 8)[0],
            struct.unpack_from("<Q", data, offset + 16)[0],
            struct.unpack_from("<Q", data, offset + 32)[0],
            struct.unpack_from("<Q", data, offset + 40)[0],
        ))
    return result


def virtual_to_file(address: int, size: int, segments: list[tuple[int, int, int, int]]) -> int:
    for p_offset, p_vaddr, p_filesz, _ in segments:
        if p_vaddr <= address and address + size <= p_vaddr + p_filesz:
            return p_offset + (address - p_vaddr)
    raise ValueError(f"address 0x{address:x} is not file-backed by PT_LOAD")


def parse_pattern(text: str) -> tuple[bytes, bytes]:
    values = []
    masks = []
    for token in text.split():
        if token in {"?", "??"}:
            values.append(0)
            masks.append(0)
        else:
            values.append(int(token, 16))
            masks.append(0xFF)
    if not values or 0 not in masks:
        raise ValueError(f"pattern is not wildcarded: {text}")
    return bytes(values), bytes(masks)


def pattern_matches_at(data: bytes, offset: int, values: bytes, masks: bytes) -> bool:
    if offset < 0 or offset + len(values) > len(data):
        return False
    return all((data[offset+i] & masks[i]) == (values[i] & masks[i]) for i in range(len(values)))


def scan_pattern(data: bytes, values: bytes, masks: bytes) -> list[int]:
    # Search by the longest contiguous exact-byte anchor, then verify the full
    # masked pattern. This keeps uniqueness scans fast even on multi-megabyte ELFs.
    best_start = best_len = 0
    run_start = run_len = 0
    for index, mask in enumerate(masks):
        if mask == 0xFF:
            if run_len == 0:
                run_start = index
            run_len += 1
            if run_len > best_len:
                best_start, best_len = run_start, run_len
        else:
            run_len = 0
    if best_len == 0:
        raise ValueError("wildcard pattern has no exact anchor")
    anchor = values[best_start:best_start + best_len]
    hits = []
    search_from = 0
    while True:
        pos = data.find(anchor, search_from)
        if pos < 0:
            break
        candidate = pos - best_start
        if pattern_matches_at(data, candidate, values, masks):
            hits.append(candidate)
        search_from = pos + 1
    return hits


def dwarf_name(line: str) -> str:
    value = line.split("DW_AT_name", 1)[1].split(":", 1)[1].strip()
    if "): " in value:
        value = value.rsplit("): ", 1)[1]
    elif ": " in value:
        value = value.rsplit(": ", 1)[1]
    return value.strip()


def trailing_int(line: str) -> int | None:
    match = re.search(r"(-?\d+)\s*$", line)
    return int(match.group(1)) if match else None


def verify_dwarf_layouts(path: pathlib.Path) -> dict[str, object]:
    header_re = re.compile(r"^\s*<(\d+)><[^>]+>:\s+Abbrev Number: \d+ \((DW_TAG_[^)]+)\)")
    candidates = {name: [] for name in EXPECTED_LAYOUTS}
    current = None
    active = None

    def finish_current() -> None:
        nonlocal current, active
        if not current:
            return
        tag = current["tag"]
        level = int(current["level"])
        name = current.get("name")
        if tag in {"DW_TAG_structure_type", "DW_TAG_class_type"} and name in EXPECTED_LAYOUTS:
            active = {"name": name, "level": level, "size": current.get("size"), "members": {}}
        elif active and tag == "DW_TAG_member" and level == int(active["level"]) + 1:
            member_name = current.get("name")
            member_offset = current.get("member_offset")
            if isinstance(member_name, str) and isinstance(member_offset, int):
                active["members"][member_name] = member_offset
        current = None

    def close_active() -> None:
        nonlocal active
        if active:
            candidates[str(active["name"])].append(active)
            active = None

    process = subprocess.Popen(
        ["readelf", "--debug-dump=info", "--wide", str(path)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, errors="replace")
    assert process.stdout is not None
    for line in process.stdout:
        header = header_re.match(line)
        if header:
            finish_current()
            level = int(header.group(1))
            if active and level <= int(active["level"]):
                close_active()
            current = {"level": level, "tag": header.group(2)}
            continue
        if not current:
            continue
        if "DW_AT_name" in line:
            current["name"] = dwarf_name(line)
        elif "DW_AT_byte_size" in line:
            value = trailing_int(line)
            if value is not None:
                current["size"] = value
        elif "DW_AT_data_member_location" in line:
            value = trailing_int(line)
            if value is not None:
                current["member_offset"] = value
    finish_current(); close_active()
    stderr = process.stderr.read() if process.stderr else ""
    if process.wait() != 0:
        raise ValueError(f"readelf DWARF parse failed: {stderr.strip()}")

    verified = {}
    for name, expected in EXPECTED_LAYOUTS.items():
        members = expected["members"]
        match = next((c for c in candidates[name]
                      if c.get("size") == expected["size"] and
                      all(c.get("members", {}).get(m) == off for m, off in members.items())), None)
        if not match:
            raise ValueError(f"DWARF layout mismatch for {name}: {candidates[name]}")
        verified[name] = {"size": match["size"], "members": {m: match["members"][m] for m in members}}
    return verified


def main() -> int:
    if len(sys.argv) != 2:
        raise ValueError("usage: verify-target-abi.py <libpreloader.so>")
    path = pathlib.Path(sys.argv[1]).resolve()
    data = path.read_bytes()
    expected = TARGET["levilauncher"]
    actual_sha = hashlib.sha256(data).hexdigest()
    actual_build_id = build_id(path)
    if len(data) != expected["size"]: raise ValueError(f"preloader size mismatch: {len(data)}")
    if actual_sha != expected["sha256"]: raise ValueError(f"preloader SHA-256 mismatch: {actual_sha}")
    if actual_build_id != expected["gnu_build_id"]: raise ValueError(f"preloader Build ID mismatch: {actual_build_id}")

    symbol_map = symbols(path)
    missing = sorted(REQUIRED_GLOSS.difference(symbol_map))
    if missing: raise ValueError(f"missing Gloss exports: {missing}")
    segments = load_segments(data)
    layouts = verify_dwarf_layouts(path)
    verified = {}

    for short_name, descriptor in TARGET["bridge_abi"].items():
        name = JNI_PREFIX + short_name
        if name not in symbol_map: raise ValueError(f"missing bridge symbol: {name}")
        value, size = symbol_map[name]
        expected_size = int(descriptor["size"])
        if size != expected_size: raise ValueError(f"{short_name} size mismatch: {size} != {expected_size}")
        file_offset = virtual_to_file(value, max(4, min(size, 32)), segments)
        entry = {
            "rva": f"0x{value:x}", "file_offset": f"0x{file_offset:x}", "size": size,
            "observed_first_16_bytes": data[file_offset:file_offset+min(16,size)].hex(" ").upper(),
        }
        pattern_text = descriptor.get("wildcard_signature")
        if pattern_text:
            values, masks = parse_pattern(pattern_text)
            if not pattern_matches_at(data, file_offset, values, masks):
                raise ValueError(f"{short_name} wildcard signature does not match symbol entry")
            hits = scan_pattern(data, values, masks)
            expected_count = int(descriptor.get("match_count", 1))
            if len(hits) != expected_count:
                raise ValueError(f"{short_name} wildcard signature matches {len(hits)}, expected {expected_count}")
            if file_offset not in hits:
                raise ValueError(f"{short_name} wildcard signature unique hit is not the symbol entry")
            entry["wildcard_signature"] = pattern_text
            entry["match_count"] = len(hits)
            entry["match_file_offsets"] = [f"0x{x:x}" for x in hits]
        elif short_name.endswith("Revision") or short_name.endswith("Count"):
            instruction = struct.unpack_from("<I", data, file_offset)[0]
            if (instruction & 0x7C000000) != 0x14000000:
                raise ValueError(f"{short_name} is not an AArch64 direct B stub")
            entry["validation"] = "AArch64 direct B opcode"
        verified[short_name] = entry

    revision_stub = verified["nativeGetDrawCommandsRevision"]
    stub_rva = int(revision_stub["rva"], 16)
    stub_offset = int(revision_stub["file_offset"], 16)
    instruction = struct.unpack_from("<I", data, stub_offset)[0]
    displacement = instruction & 0x03FFFFFF
    if displacement & 0x02000000: displacement -= 1 << 26
    target_rva = stub_rva + (displacement << 2)
    plan = TARGET["runtime_hook_plan"]["draw_revision_target"]
    if target_rva != int(plan["target_rva"], 16):
        raise ValueError(f"draw revision target mismatch: 0x{target_rva:x} != {plan['target_rva']}")
    values, masks = parse_pattern(plan["wildcard_signature"])
    target_offset = virtual_to_file(target_rva, len(values), segments)
    if not pattern_matches_at(data, target_offset, values, masks):
        raise ValueError("draw revision target wildcard signature mismatch")
    hits = scan_pattern(data, values, masks)
    if len(hits) != int(plan.get("match_count", 1)) or target_offset not in hits:
        raise ValueError(f"draw revision target wildcard signature matches {len(hits)} locations")

    result = {
        "status":"PASS", "preloader":str(path), "size":len(data), "sha256":actual_sha,
        "gnu_build_id":actual_build_id, "elf":"ELF64 little-endian AArch64 ET_DYN",
        "required_gloss_exports":sorted(REQUIRED_GLOSS), "verified_bridge_symbols":verified,
        "verified_branch_targets":{"nativeGetDrawCommandsRevision":{
            "stub_rva":f"0x{stub_rva:x}", "target_rva":f"0x{target_rva:x}",
            "target_file_offset":f"0x{target_offset:x}", "target_size":plan["target_size"],
            "wildcard_signature":plan["wildcard_signature"], "match_count":len(hits),
        }},
        "verified_dwarf_layouts":layouts,
        "runtime_layout_derivation":{"ModContext.mInfo":layouts["ModContext"]["members"]["mInfo"],
            "ModInfo.modRootPath":layouts["ModInfo"]["members"]["modRootPath"],
            "effective_mod_root_path_offset_in_context":224},
    }
    output = ROOT / "reports/target_abi_verification.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2)+"\n")
    print(json.dumps(result, indent=2))
    return 0

if __name__ == "__main__":
    try: raise SystemExit(main())
    except Exception as exc:
        print(f"target ABI verification failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
