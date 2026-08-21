#!/usr/bin/env python3
"""Build a UE 5.8 LocRes v3 (Optimized_CityHash64_UTF16) from extracted LOCTEXT + zh-Hans translations.
Self-contained: CityHash64 + str_crc32 inlined verbatim from pylocres (validated against real UE files)."""
import struct, json

# ================= CRC32 (verbatim from pylocres crc_hash.py) =================
CRCTable = [
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
]

def str_crc32(value, CRC=0):
    CRC = ~CRC & 0xFFFFFFFF
    for i in range(len(value)):
        ch = ord(value[i])
        CRC = (CRC >> 8) ^ CRCTable[(CRC ^ ch) & 0xFF]
        ch >>= 8
        CRC = (CRC >> 8) ^ CRCTable[(CRC ^ ch) & 0xFF]
        ch >>= 8
        CRC = (CRC >> 8) ^ CRCTable[(CRC ^ ch) & 0xFF]
        ch >>= 8
        CRC = (CRC >> 8) ^ CRCTable[(CRC ^ ch) & 0xFF]
    return ~CRC & 0xFFFFFFFF

# ================= CityHash64 (verbatim from pylocres city_hash.py) =================
K0 = 0xc3a5c85c97cb3127; K1 = 0xb492b66fbe98f273; K2 = 0x9ae16a3b2f90404f; K3 = 0xc949d7c7509e6557
C1 = 0xcc9e2d51; C2 = 0x1b873593; HASH_MUL = 0x9ddfea08eb382d69; BIG_ENDIAN = False

class CityHash:
    @staticmethod
    def _uint64(x): return x & 0xFFFFFFFFFFFFFFFF
    @staticmethod
    def _uint32(x): return x & 0xFFFFFFFF
    @staticmethod
    def byte_swap_uint32(x):
        x = CityHash._uint32(x)
        return ((x >> 24) | ((x & 0x00ff0000) >> 8) | ((x & 0x0000ff00) << 8) | (x << 24))
    @staticmethod
    def byte_swap_uint64(x):
        x = CityHash._uint64(x)
        return ((x >> 56) | ((x & 0x00ff000000000000) >> 40) | ((x & 0x0000ff0000000000) >> 24) |
                ((x & 0x000000ff00000000) >> 8) | ((x & 0x00000000ff000000) << 8) |
                ((x & 0x0000000000ff0000) << 24) | ((x & 0x000000000000ff00) << 40) | (x << 56))
    @staticmethod
    def hash128_to_64(x):
        a = CityHash._uint64((x[0] ^ x[1]) * HASH_MUL); a ^= (a >> 47)
        b = CityHash._uint64((x[1] ^ a) * HASH_MUL); b ^= (b >> 47)
        return CityHash._uint64(b * HASH_MUL)
    @staticmethod
    def fetch32(data, index):
        x = struct.unpack_from('<I', data, index)[0]
        return CityHash._uint64(CityHash.byte_swap_uint32(x) if BIG_ENDIAN else x)
    @staticmethod
    def fetch64(data, index):
        x = struct.unpack_from('<Q', data, index)[0]
        return CityHash._uint64(CityHash.byte_swap_uint64(x) if BIG_ENDIAN else x)
    @staticmethod
    def rotate(val, shift):
        val = CityHash._uint64(val)
        if shift == 0: return CityHash._uint64(val)
        return CityHash._uint64((val >> shift) | (val << (64 - shift)))
    @staticmethod
    def shift_mix(val):
        val = CityHash._uint64(val)
        return CityHash._uint64(val ^ (val >> 47))
    @staticmethod
    def hash_len16(u, v, mul=None):
        if mul is None: return CityHash.hash128_to_64([u, v])
        u, v = CityHash._uint64(u), CityHash._uint64(v); mul = CityHash._uint64(mul)
        a = CityHash._uint64((u ^ v) * mul); a ^= (a >> 47)
        b = CityHash._uint64((v ^ a) * mul); b ^= (b >> 47)
        return CityHash._uint64(b * mul)
    @staticmethod
    def hash_len0_to_16(data, offset):
        length = len(data) - offset
        if length >= 8:
            mul = K2 + length * 2
            a = CityHash.fetch64(data, offset) + K2
            b = CityHash.fetch64(data, offset + length - 8)
            c = CityHash._uint64(CityHash.rotate(b, 37) * mul + a)
            d = CityHash._uint64((CityHash.rotate(a, 25) + b) * mul)
            return CityHash.hash_len16(c, d, mul)
        if length >= 4:
            mul = K2 + length * 2
            a = CityHash.fetch32(data, offset)
            return CityHash.hash_len16(length + (a << 3), CityHash.fetch32(data, offset + length - 4), mul)
        if length > 0:
            a = data[offset]; b = data[offset + (length >> 1)]; c = data[offset + (length - 1)]
            y = a + (b << 8); z = length + (c << 2)
            return CityHash._uint64(CityHash.shift_mix((y * K2) ^ (z * K0)) * K2)
        return K2
    @staticmethod
    def hash_len17_to_32(data):
        length = len(data); mul = CityHash._uint64(K2 + length * 2)
        a = CityHash._uint64(CityHash.fetch64(data, 0) * K1)
        b = CityHash.fetch64(data, 8)
        c = CityHash._uint64(CityHash.fetch64(data, length - 8) * mul)
        d = CityHash._uint64(CityHash.fetch64(data, length - 16) * K2)
        return CityHash.hash_len16(
            CityHash._uint64(CityHash.rotate(a + b, 43) + CityHash.rotate(c, 30) + d),
            CityHash._uint64(a + CityHash.rotate(b + K2, 18) + c), mul)
    @staticmethod
    def hash_len33_to_64(data):
        length = len(data); mul = CityHash._uint64(K2 + length * 2)
        a = CityHash._uint64(CityHash.fetch64(data, 0) * K2); b = CityHash.fetch64(data, 8)
        c = CityHash.fetch64(data, length - 24); d = CityHash.fetch64(data, length - 32)
        e = CityHash._uint64(CityHash.fetch64(data, 16) * K2); f = CityHash._uint64(CityHash.fetch64(data, 24) * 9)
        g = CityHash.fetch64(data, length - 8); h = CityHash._uint64(CityHash.fetch64(data, length - 16) * mul)
        u = CityHash._uint64(CityHash.rotate(a + g, 43) + (CityHash.rotate(b, 30) + c) * 9)
        v = CityHash._uint64((a + g) ^ d) + f + 1
        w = CityHash._uint64(CityHash.byte_swap_uint64((u + v) * mul) + h)
        x = CityHash._uint64(CityHash.rotate(e + f, 42) + c)
        y = CityHash._uint64(CityHash.byte_swap_uint64((v + w) * mul) + g) * mul
        z = CityHash._uint64(e + f + c)
        a = CityHash._uint64(CityHash.byte_swap_uint64((x + z) * mul + y) + b)
        b = CityHash._uint64(CityHash.shift_mix((z + a) * mul + d + h) * mul)
        return CityHash._uint64(b + x)
    @staticmethod
    def weak_hash_len32_with_seeds(w, x, y, z, a, b):
        a += w; b = CityHash.rotate(b + a + z, 21); c = a
        a += x; a += y; b += CityHash.rotate(a, 44)
        return [CityHash._uint64(a + z), CityHash._uint64(b + c)]
    @staticmethod
    def weak_hash_len32_with_seeds_from_bytes(data, offset, a, b):
        return CityHash.weak_hash_len32_with_seeds(
            CityHash.fetch64(data, offset), CityHash.fetch64(data, offset + 8),
            CityHash.fetch64(data, offset + 16), CityHash.fetch64(data, offset + 24), a, b)
    @staticmethod
    def city_hash_64_utf16_to_uint32(s):
        if not s: return 0
        h = CityHash.city_hash_64(s)
        return CityHash._uint32((h & 0xFFFFFFFF) + (((h >> 32) & 0xFFFFFFFF) * 23))
    @staticmethod
    def city_hash_64(s):
        data = s.encode('utf-16le'); length = len(data)
        if length <= 32:
            if length <= 16: return CityHash.hash_len0_to_16(data, 0)
            return CityHash.hash_len17_to_32(data)
        if length <= 64: return CityHash.hash_len33_to_64(data)
        x = CityHash.fetch64(data, length - 40)
        y = CityHash.fetch64(data, length - 16) + CityHash.fetch64(data, length - 56)
        z = CityHash.hash_len16(CityHash.fetch64(data, length - 48) + length, CityHash.fetch64(data, length - 24))
        v = CityHash.weak_hash_len32_with_seeds_from_bytes(data, length - 64, length, z)
        w = CityHash.weak_hash_len32_with_seeds_from_bytes(data, length - 32, y + K1, x)
        x = CityHash._uint64(x * K1 + CityHash.fetch64(data, 0))
        chunk_length = (len(data) - 1) & ~63
        offset = 0
        while chunk_length != 0:
            x = CityHash._uint64(CityHash.rotate(x + y + v[0] + CityHash.fetch64(data, offset + 8), 37) * K1)
            y = CityHash._uint64(CityHash.rotate(y + v[1] + CityHash.fetch64(data, offset + 48), 42) * K1)
            x ^= w[1]
            y = CityHash._uint64(y + v[0] + CityHash.fetch64(data, offset + 40))
            z = CityHash._uint64(CityHash.rotate(z + w[0], 33) * K1)
            v = CityHash.weak_hash_len32_with_seeds_from_bytes(data, offset, v[1] * K1, x + w[0])
            w = CityHash.weak_hash_len32_with_seeds_from_bytes(data, offset + 32, z + w[1], y + CityHash.fetch64(data, offset + 16))
            z, x = x, z
            offset += 64; chunk_length -= 64
        return CityHash.hash_len16(
            CityHash.hash_len16(v[0], w[0]) + CityHash.shift_mix(y) * K1 + z,
            CityHash.hash_len16(v[1], w[1]) + x)

# ================= LocRes v3 writer =================
MAGIC = bytes.fromhex("0e147475674a03fc4a15909dc3377f1b")

def fstring(s: str) -> bytes:
    if s.isascii():
        return struct.pack("<i", len(s) + 1) + s.encode("ascii") + b"\x00"
    b = s.encode("utf-16le")
    return struct.pack("<i", -(len(b) // 2 + 1)) + b + b"\x00\x00"

def build_locres(entries, out_path):
    """entries: list of (namespace, key, source, translation)"""
    lut_map, lut = {}, []
    def lut_idx(s):
        if s not in lut_map:
            lut_map[s] = len(lut); lut.append([s, 0])
        i = lut_map[s]; lut[i][1] += 1; return i
    out = bytearray(MAGIC + bytes([3]) + struct.pack("<q", -1))
    ns_groups = {}
    for ns, key, src, tr in entries:
        ns_groups.setdefault(ns, []).append((key, src, tr))
    out += struct.pack("<II", len(entries), len(ns_groups))
    for ns, keys in ns_groups.items():
        out += struct.pack("<I", CityHash.city_hash_64_utf16_to_uint32(ns)) + fstring(ns)
        out += struct.pack("<I", len(keys))
        for key, src, tr in keys:
            out += struct.pack("<I", CityHash.city_hash_64_utf16_to_uint32(key)) + fstring(key)
            out += struct.pack("<I", str_crc32(src))
            out += struct.pack("<i", lut_idx(tr))
    lut_off = len(out)
    out += struct.pack("<I", len(lut))
    for s, ref in lut:
        out += fstring(s) + struct.pack("<i", ref)
    struct.pack_into("<q", out, 17, lut_off)
    with open(out_path, "wb") as f:
        f.write(bytes(out))
    return len(entries), len(ns_groups), len(lut)

# ================= Independent reader (verification) =================
def read_fstring(buf, off):
    n = struct.unpack_from("<i", buf, off)[0]
    if n > 0: return buf[off+4:off+4+n].decode("ascii", errors="replace").rstrip("\x00"), off + 4 + n
    if n < 0: return buf[off+4:off+4-n*2].decode("utf-16le", errors="replace").rstrip("\x00"), off + 4 - n*2
    return "", off + 4

def read_locres(path):
    buf = open(path, "rb").read()
    assert buf[:16] == MAGIC, "magic mismatch"
    ver = buf[16]
    assert ver == 3, f"version {ver}"
    lut_off = struct.unpack_from("<q", buf, 17)[0]
    nents = struct.unpack_from("<I", buf, 25)[0]
    nns = struct.unpack_from("<I", buf, 29)[0]
    off = 33
    entries = []
    for _ in range(nns):
        ns_hash = struct.unpack_from("<I", buf, off)[0]; off += 4
        ns, off = read_fstring(buf, off)
        nk = struct.unpack_from("<I", buf, off)[0]; off += 4
        for _ in range(nk):
            key_hash = struct.unpack_from("<I", buf, off)[0]; off += 4
            key, off = read_fstring(buf, off)
            src_crc = struct.unpack_from("<I", buf, off)[0]; off += 4
            idx = struct.unpack_from("<i", buf, off)[0]; off += 4
            entries.append((ns, key, src_crc, idx))
    assert off == lut_off, f"keys end {off} != lut_off {lut_off}"
    nlut = struct.unpack_from("<I", buf, off)[0]; off += 4
    strings = []
    for _ in range(nlut):
        s, off = read_fstring(buf, off)
        ref = struct.unpack_from("<i", buf, off)[0]; off += 4
        strings.append(s)
    return ver, nents, nns, entries, strings, off

if __name__ == "__main__":
    import os
    _BASE = os.path.dirname(os.path.abspath(__file__))
    loctext = json.load(open(os.path.join(_BASE, "loctext.json"), encoding="utf-8"))
    trans = json.load(open(os.path.join(_BASE, "translations.json"), encoding="utf-8"))
    entries = []
    missing = 0
    for e in loctext:
        tr = trans.get(e["ns"] + "|" + e["key"], "")
        if not tr: missing += 1
        entries.append((e["ns"], e["key"], e["src"], tr))
    print(f"input entries: {len(entries)}, missing translations: {missing}")

    out_path = os.path.join(_BASE, "DreamShader.locres")
    n, nns, nlut = build_locres(entries, out_path)
    print(f"built: entries={n} namespaces={nns} lut_strings={nlut} -> {out_path}")

    ver, rn, rns, rents, rstrings, rend = read_locres(out_path)
    print(f"read-back: version={ver} entries={rn} namespaces={rns} lut={len(rstrings)} end={rend}")
    assert rn == len(entries) and rns == nns
    # spot check a few entries resolve to the right translation
    bykey = { (ns, k): (src_crc, idx) for ns, k, src_crc, idx in rents }
    for ns, k, src, tr in entries[:8]:
        src_crc, idx = bykey[(ns, k)]
        assert src_crc == str_crc32(src), f"crc mismatch for {ns}.{k}"
        assert rstrings[idx] == tr, f"translation mismatch for {ns}.{k}: {rstrings[idx]!r} vs {tr!r}"
    print("spot-check passed (crc + translation resolved correctly)")
    # full check: every translation resolves
    ok = all(rstrings[idx] == tr for (ns, k, src, tr), (_, _, _, idx) in
             [(e, r) for e, r in zip(entries, rents)])
    # zip-based full verify
    full_ok = True
    for e, r in zip(entries, rents):
        if rstrings[r[3]] != e[3]:
            full_ok = False; print("MISMATCH", e[0], e[1], rstrings[r[3]], e[3])
    print(f"full translation resolve: {'OK' if full_ok else 'FAIL'}")
    print(f"file size: {len(open(out_path,'rb').read())} bytes")

