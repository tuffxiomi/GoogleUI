#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import pathlib
import struct
import zlib
from dataclasses import dataclass

NO_INDEX = 0xFFFFFFFF


def align(value: int, alignment: int = 4) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def uleb128(value: int) -> bytes:
    if value < 0:
        raise ValueError(value)
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def p16(value: int) -> bytes:
    return struct.pack('<H', value)


def p32(value: int) -> bytes:
    return struct.pack('<I', value)


@dataclass(frozen=True)
class Proto:
    shorty: str
    return_desc: str
    params: tuple[str, ...]


@dataclass(frozen=True)
class Method:
    class_desc: str
    name: str
    proto: Proto


CLASS_DESC = 'Lorg/levimc/googleui/AdBlockWebViewClient;'
WEBVIEW_CLIENT = 'Landroid/webkit/WebViewClient;'
WEBVIEW = 'Landroid/webkit/WebView;'
REQUEST = 'Landroid/webkit/WebResourceRequest;'
RESPONSE = 'Landroid/webkit/WebResourceResponse;'
URI = 'Landroid/net/Uri;'
STRING = 'Ljava/lang/String;'
INPUT_STREAM = 'Ljava/io/InputStream;'
BAIS = 'Ljava/io/ByteArrayInputStream;'
BYTE_ARRAY = '[B'
VOID = 'V'
BOOLEAN = 'Z'

P_VOID = Proto('V', VOID, ())
P_BLOCK = Proto('ZL', BOOLEAN, (STRING,))
P_INTERCEPT = Proto('LLL', RESPONSE, (WEBVIEW, REQUEST))
P_URI = Proto('L', URI, ())
P_STRING = Proto('L', STRING, ())
P_BAIS_INIT = Proto('VL', VOID, (BYTE_ARRAY,))
P_RESPONSE_INIT = Proto('VLLL', VOID, (STRING, STRING, INPUT_STREAM))

OUR_INIT = Method(CLASS_DESC, '<init>', P_VOID)
OUR_BLOCK = Method(CLASS_DESC, 'nativeShouldBlock', P_BLOCK)
OUR_INTERCEPT = Method(CLASS_DESC, 'shouldInterceptRequest', P_INTERCEPT)
SUPER_INIT = Method(WEBVIEW_CLIENT, '<init>', P_VOID)
SUPER_INTERCEPT = Method(WEBVIEW_CLIENT, 'shouldInterceptRequest', P_INTERCEPT)
REQUEST_GET_URL = Method(REQUEST, 'getUrl', P_URI)
URI_TO_STRING = Method(URI, 'toString', P_STRING)
BAIS_INIT = Method(BAIS, '<init>', P_BAIS_INIT)
RESPONSE_INIT = Method(RESPONSE, '<init>', P_RESPONSE_INIT)

METHODS = [
    OUR_INIT,
    OUR_BLOCK,
    OUR_INTERCEPT,
    SUPER_INIT,
    SUPER_INTERCEPT,
    REQUEST_GET_URL,
    URI_TO_STRING,
    BAIS_INIT,
    RESPONSE_INIT,
]
PROTOS = [P_VOID, P_BLOCK, P_INTERCEPT, P_URI, P_STRING, P_BAIS_INIT, P_RESPONSE_INIT]
TYPES = {
    CLASS_DESC,
    WEBVIEW_CLIENT,
    WEBVIEW,
    REQUEST,
    RESPONSE,
    URI,
    STRING,
    INPUT_STREAM,
    BAIS,
    BYTE_ARRAY,
    VOID,
    BOOLEAN,
}
STRINGS = set(TYPES)
for proto in PROTOS:
    STRINGS.add(proto.shorty)
for method in METHODS:
    STRINGS.add(method.name)
STRINGS.update({'text/plain', 'utf-8'})


def encode_invoke(opcode: int, registers: list[int], method_idx: int) -> list[int]:
    if len(registers) > 5 or any(register < 0 or register > 15 for register in registers):
        raise ValueError(registers)
    regs = registers + [0] * (5 - len(registers))
    c, d, e, f, g = regs
    first = opcode | (g << 8) | (len(registers) << 12)
    third = c | (d << 4) | (e << 8) | (f << 12)
    return [first, method_idx, third]


def code_constructor(method_idx: dict[Method, int]) -> bytes:
    insns = []
    insns += encode_invoke(0x70, [0], method_idx[SUPER_INIT])
    insns += [0x000E]
    header = struct.pack('<HHHHII', 1, 1, 1, 0, 0, len(insns))
    return header + struct.pack('<' + 'H' * len(insns), *insns)


def code_intercept(method_idx: dict[Method, int], type_idx: dict[str, int], string_idx: dict[str, int]) -> bytes:
    # registers v0..v7; p0=v5, p1=v6, p2=v7
    insns: list[int] = []
    insns += encode_invoke(0x72, [7], method_idx[REQUEST_GET_URL])
    insns += [0x000C]  # move-result-object v0
    insns += encode_invoke(0x6E, [0], method_idx[URI_TO_STRING])
    insns += [0x010C]  # move-result-object v1
    insns += encode_invoke(0x71, [1], method_idx[OUR_BLOCK])
    insns += [0x020A]  # move-result v2
    branch_index = len(insns)
    insns += [0x0238, 0]  # if-eqz v2, allow

    insns += [0x0022, type_idx[BAIS]]                    # new-instance v0
    insns += [0x0112]                                    # const/4 v1, #0
    insns += [0x1123, type_idx[BYTE_ARRAY]]              # new-array v1, v1, [B
    insns += encode_invoke(0x70, [0, 1], method_idx[BAIS_INIT])
    insns += [0x0122, type_idx[RESPONSE]]                # new-instance v1
    insns += [0x021A, string_idx['text/plain']]          # const-string v2
    insns += [0x031A, string_idx['utf-8']]               # const-string v3
    insns += encode_invoke(0x70, [1, 2, 3, 0], method_idx[RESPONSE_INIT])
    insns += [0x0111]                                    # return-object v1

    allow_index = len(insns)
    insns[branch_index + 1] = (allow_index - branch_index) & 0xFFFF
    insns += encode_invoke(0x6F, [5, 6, 7], method_idx[SUPER_INTERCEPT])
    insns += [0x000C, 0x0011]                            # move-result-object v0; return-object v0

    header = struct.pack('<HHHHII', 8, 3, 4, 0, 0, len(insns))
    return header + struct.pack('<' + 'H' * len(insns), *insns)


def generate_dex() -> bytes:
    strings = sorted(STRINGS)
    string_idx = {value: index for index, value in enumerate(strings)}

    types = sorted(TYPES, key=lambda desc: string_idx[desc])
    type_idx = {value: index for index, value in enumerate(types)}

    protos = sorted(
        set(PROTOS),
        key=lambda proto: (
            type_idx[proto.return_desc],
            tuple(type_idx[value] for value in proto.params),
            string_idx[proto.shorty],
        ),
    )
    proto_idx = {value: index for index, value in enumerate(protos)}

    methods = sorted(
        METHODS,
        key=lambda method: (
            type_idx[method.class_desc],
            string_idx[method.name],
            proto_idx[method.proto],
        ),
    )
    method_idx = {value: index for index, value in enumerate(methods)}

    header_size = 0x70
    string_ids_off = header_size
    type_ids_off = string_ids_off + len(strings) * 4
    proto_ids_off = type_ids_off + len(types) * 4
    method_ids_off = proto_ids_off + len(protos) * 12
    class_defs_off = method_ids_off + len(methods) * 8
    data_off = align(class_defs_off + 32)

    data = bytearray()
    param_offsets: dict[tuple[str, ...], int] = {}
    first_type_list_off = 0
    for params in sorted({proto.params for proto in protos if proto.params}):
        absolute = align(data_off + len(data))
        if absolute > data_off + len(data):
            data.extend(b'\0' * (absolute - (data_off + len(data))))
        if not first_type_list_off:
            first_type_list_off = absolute
        param_offsets[params] = absolute
        data += p32(len(params))
        for desc in params:
            data += p16(type_idx[desc])
        if len(params) & 1:
            data += b'\0\0'

    absolute = align(data_off + len(data))
    data.extend(b'\0' * (absolute - (data_off + len(data))))
    constructor_code_off = absolute
    constructor_code = code_constructor(method_idx)
    data += constructor_code

    absolute = align(data_off + len(data))
    data.extend(b'\0' * (absolute - (data_off + len(data))))
    intercept_code_off = absolute
    intercept_code = code_intercept(method_idx, type_idx, string_idx)
    data += intercept_code

    string_data_offsets: dict[str, int] = {}
    first_string_data_off = data_off + len(data)
    for value in strings:
        string_data_offsets[value] = data_off + len(data)
        encoded = value.encode('utf-8')
        data += uleb128(len(value)) + encoded + b'\0'

    class_data_off = data_off + len(data)
    direct = [
        (method_idx[OUR_INIT], 0x10001, constructor_code_off),
        (method_idx[OUR_BLOCK], 0x109, 0),
    ]
    direct.sort()
    virtual = [(method_idx[OUR_INTERCEPT], 0x1, intercept_code_off)]
    class_data = bytearray()
    class_data += uleb128(0) + uleb128(0) + uleb128(len(direct)) + uleb128(len(virtual))
    previous = 0
    for index, flags, code_off in direct:
        class_data += uleb128(index - previous) + uleb128(flags) + uleb128(code_off)
        previous = index
    previous = 0
    for index, flags, code_off in virtual:
        class_data += uleb128(index - previous) + uleb128(flags) + uleb128(code_off)
        previous = index
    data += class_data

    map_off = align(data_off + len(data))
    data.extend(b'\0' * (map_off - (data_off + len(data))))

    sections = [
        (0x0000, 1, 0),
        (0x0001, len(strings), string_ids_off),
        (0x0002, len(types), type_ids_off),
        (0x0003, len(protos), proto_ids_off),
        (0x0005, len(methods), method_ids_off),
        (0x0006, 1, class_defs_off),
    ]
    if param_offsets:
        sections.append((0x1001, len(param_offsets), first_type_list_off))
    sections.extend([
        (0x2001, 2, constructor_code_off),
        (0x2002, len(strings), first_string_data_off),
        (0x2000, 1, class_data_off),
        (0x1000, 1, map_off),
    ])
    sections.sort(key=lambda item: item[2])
    map_blob = bytearray(p32(len(sections)))
    for item_type, count, offset in sections:
        map_blob += struct.pack('<HHII', item_type, 0, count, offset)
    data += map_blob

    file_size = data_off + len(data)
    output = bytearray(file_size)

    output[0:8] = b'dex\n035\0'
    struct.pack_into(
        '<20I',
        output,
        32,
        file_size,
        header_size,
        0x12345678,
        0,
        0,
        map_off,
        len(strings),
        string_ids_off,
        len(types),
        type_ids_off,
        len(protos),
        proto_ids_off,
        0,
        0,
        len(methods),
        method_ids_off,
        1,
        class_defs_off,
        len(data),
        data_off,
    )

    for index, value in enumerate(strings):
        struct.pack_into('<I', output, string_ids_off + index * 4, string_data_offsets[value])
    for index, value in enumerate(types):
        struct.pack_into('<I', output, type_ids_off + index * 4, string_idx[value])
    for index, proto in enumerate(protos):
        struct.pack_into(
            '<III',
            output,
            proto_ids_off + index * 12,
            string_idx[proto.shorty],
            type_idx[proto.return_desc],
            param_offsets.get(proto.params, 0),
        )
    for index, method in enumerate(methods):
        struct.pack_into(
            '<HHI',
            output,
            method_ids_off + index * 8,
            type_idx[method.class_desc],
            proto_idx[method.proto],
            string_idx[method.name],
        )
    struct.pack_into(
        '<8I',
        output,
        class_defs_off,
        type_idx[CLASS_DESC],
        0x11,
        type_idx[WEBVIEW_CLIENT],
        0,
        NO_INDEX,
        0,
        class_data_off,
        0,
    )
    output[data_off:] = data

    output[12:32] = hashlib.sha1(output[32:]).digest()
    struct.pack_into('<I', output, 8, zlib.adler32(output[12:]) & 0xFFFFFFFF)
    validate_dex(bytes(output), CLASS_DESC)
    return bytes(output)


def read_uleb(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    shift = 0
    while True:
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7F) << shift
        if not (byte & 0x80):
            return value, offset
        shift += 7
        if shift > 35:
            raise ValueError('invalid ULEB128')


def validate_dex(data: bytes, required_descriptor: str) -> None:
    if data[:8] != b'dex\n035\0':
        raise ValueError('invalid DEX magic')
    checksum = struct.unpack_from('<I', data, 8)[0]
    if checksum != (zlib.adler32(data[12:]) & 0xFFFFFFFF):
        raise ValueError('invalid DEX checksum')
    if data[12:32] != hashlib.sha1(data[32:]).digest():
        raise ValueError('invalid DEX signature')
    file_size, header_size, endian = struct.unpack_from('<III', data, 32)
    if file_size != len(data) or header_size != 0x70 or endian != 0x12345678:
        raise ValueError('invalid DEX header')
    string_count, string_off = struct.unpack_from('<II', data, 56)
    strings = []
    for index in range(string_count):
        item_off = struct.unpack_from('<I', data, string_off + index * 4)[0]
        _, cursor = read_uleb(data, item_off)
        end = data.index(0, cursor)
        strings.append(data[cursor:end].decode('utf-8'))
    if required_descriptor not in strings or 'nativeShouldBlock' not in strings or 'shouldInterceptRequest' not in strings:
        raise ValueError('required class or methods missing')


def write_cpp(dex: bytes, header_path: pathlib.Path, source_path: pathlib.Path) -> None:
    header_path.write_text(
        '#pragma once\n\n#include "CoreTypes.hpp"\n\nnamespace google_ui::adblock_dex {\n'
        'extern const u8 kDexBytes[];\nextern const usize kDexSize;\n}\n',
        encoding='utf-8',
    )
    lines = []
    for offset in range(0, len(dex), 16):
        chunk = dex[offset:offset + 16]
        lines.append('    ' + ', '.join(f'0x{value:02X}' for value in chunk) + ',')
    source_path.write_text(
        '#include "AdBlockClientDex.hpp"\n\nnamespace google_ui::adblock_dex {\n'
        'const u8 kDexBytes[] = {\n' + '\n'.join(lines) + '\n};\n'
        f'const usize kDexSize = {len(dex)};\n'
        '} // namespace google_ui::adblock_dex\n',
        encoding='utf-8',
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--dex', type=pathlib.Path, required=True)
    parser.add_argument('--header', type=pathlib.Path, required=True)
    parser.add_argument('--source', type=pathlib.Path, required=True)
    args = parser.parse_args()
    dex = generate_dex()
    args.dex.parent.mkdir(parents=True, exist_ok=True)
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.source.parent.mkdir(parents=True, exist_ok=True)
    args.dex.write_bytes(dex)
    write_cpp(dex, args.header, args.source)
    print(f'generated {args.dex} ({len(dex)} bytes, sha256={hashlib.sha256(dex).hexdigest()})')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
