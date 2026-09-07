#!/usr/bin/env python3
from __future__ import annotations
import argparse
import json
from pathlib import Path


def read_rules(path: Path) -> list[str]:
    values: list[str] = []
    seen: set[str] = set()
    for raw in path.read_text(encoding="utf-8").splitlines():
        value = raw.strip().lower()
        if not value or value.startswith("#") or value in seen:
            continue
        seen.add(value)
        values.append(value)
    return sorted(values)


def c_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def emit_bytes(name: str, data: bytes) -> str:
    values = [f"0x{byte:02X}" for byte in data + b"\0"]
    rows = ["    " + ", ".join(values[i:i + 16]) + "," for i in range(0, len(values), 16)]
    return f"alignas(16) const unsigned char {name}[] = {{\n" + "\n".join(rows) + "\n};\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--template", type=Path, required=True)
    parser.add_argument("--hosts", type=Path, required=True)
    parser.add_argument("--tokens", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--runtime-js", type=Path, required=True)
    args = parser.parse_args()

    hosts = read_rules(args.hosts)
    tokens = read_rules(args.tokens)
    raw_script = args.template.read_text(encoding="utf-8")
    raw_script = raw_script.replace("__HOSTS__", json.dumps(hosts, separators=(",", ":")))
    raw_script = raw_script.replace("__TOKENS__", json.dumps(tokens, separators=(",", ":")))
    if "__HOSTS__" in raw_script or "__TOKENS__" in raw_script:
        raise SystemExit("adblock template placeholders were not fully replaced")
    runtime_script = "javascript:" + raw_script

    lines = ['#include "AdBlockData.hpp"', '', 'namespace google_ui::adblock_data {', '']
    lines.append('const char* const kBlockedRules[] = {')
    lines.extend(f"    {c_string(value)}," for value in hosts)
    lines.append('};')
    lines.append('const usize kBlockedRuleCount = sizeof(kBlockedRules) / sizeof(kBlockedRules[0]);')
    lines.append('')
    lines.append('const char* const kBlockedUrlTokens[] = {')
    lines.extend(f"    {c_string(value)}," for value in tokens)
    lines.append('};')
    lines.append('const usize kBlockedUrlTokenCount = sizeof(kBlockedUrlTokens) / sizeof(kBlockedUrlTokens[0]);')
    lines.append('')
    lines.append(emit_bytes('kUniversalScript', runtime_script.encode('utf-8')).rstrip())
    lines.append('const usize kUniversalScriptSize = sizeof(kUniversalScript) - 1;')
    lines.append('')
    lines.append('} // namespace google_ui::adblock_data')
    lines.append('')

    args.source.parent.mkdir(parents=True, exist_ok=True)
    args.runtime_js.parent.mkdir(parents=True, exist_ok=True)
    args.source.write_text("\n".join(lines), encoding="utf-8")
    args.runtime_js.write_text(raw_script, encoding="utf-8")
    print(f"hosts={len(hosts)} tokens={len(tokens)} runtime_bytes={len(runtime_script.encode('utf-8'))}")


if __name__ == "__main__":
    main()
