#!/usr/bin/env python3
"""Désassembleur de bytecode Lua 5.0 (les .lur de Scripts/Scripts.img).

  tools/lur.py <fichier.lur>                 tout le fichier
  tools/lur.py <Scripts.img> <nom.lur>       une entrée de l'archive (lit Scripts.dir à côté)

Format Lua 5.0 (lundump.c) : en-tête « \\x1bLua », version 0x50, puis par
fonction : source, ligne, nups, nparams, vararg, maxstack, lignes, locales,
upvalues, constantes, sous-fonctions, code. Instruction 32 bits : OP 6 bits,
C 9 bits (pos 6), B 9 bits (pos 15), A 8 bits (pos 24) ; Bx = bits 6-23,
sBx = Bx - 131071 ; B ou C ≥ 250 désigne une constante (RK).
Outil de lecture : la sortie n'entre pas dans le dépôt (scripts du jeu).
"""
import struct, sys, os

OPS = ["MOVE", "LOADK", "LOADBOOL", "LOADNIL", "GETUPVAL", "GETGLOBAL", "GETTABLE", "SETGLOBAL",
       "SETUPVAL", "SETTABLE", "NEWTABLE", "SELF", "ADD", "SUB", "MUL", "DIV", "POW", "UNM", "NOT",
       "CONCAT", "JMP", "EQ", "LT", "LE", "TEST", "CALL", "TAILCALL", "RETURN", "FORLOOP",
       "TFORLOOP", "TFORPREP", "SETLIST", "SETLISTO", "CLOSE", "CLOSURE"]
MAXSTACK = 250


class R:
    def __init__(s, b): s.b, s.p = b, 0
    def u8(s): v = s.b[s.p]; s.p += 1; return v
    def i32(s): v = struct.unpack_from("<i", s.b, s.p)[0]; s.p += 4; return v
    def u32(s): v = struct.unpack_from("<I", s.b, s.p)[0]; s.p += 4; return v
    taille_nombre = 8
    def f64(s):
        if s.taille_nombre == 4: v = struct.unpack_from("<f", s.b, s.p)[0]; s.p += 4; return v
        v = struct.unpack_from("<d", s.b, s.p)[0]; s.p += 8; return v
    def chaine(s):
        n = s.u32()
        if n == 0: return None
        v = s.b[s.p:s.p + n - 1].decode("latin-1"); s.p += n; return v


def fonction(r):
    f = {}
    f["source"] = r.chaine(); f["ligne"] = r.i32()
    f["nups"], f["nparams"], f["vararg"], f["maxstack"] = r.u8(), r.u8(), r.u8(), r.u8()
    n = r.i32(); r.p += 4 * n
    n = r.i32(); f["locales"] = []
    for _ in range(n): f["locales"].append(r.chaine()); r.i32(); r.i32()
    n = r.i32(); f["upvals"] = [r.chaine() for _ in range(n)]
    n = r.i32(); f["k"] = []
    for _ in range(n):
        t = r.u8()
        f["k"].append(r.f64() if t == 3 else r.chaine() if t == 4 else None)
    n = r.i32(); f["protos"] = [fonction(r) for _ in range(n)]
    n = r.i32(); f["code"] = [r.u32() for _ in range(n)]
    return f


def rk(f, x):
    if x >= MAXSTACK:
        k = f["k"][x - MAXSTACK]
        return repr(k)
    return "r%d" % x


def montrer(f, nom="principale", prof=0):
    pad = "  " * prof
    print(f"{pad}== fonction {nom} (ligne {f['ligne']}, {f['nparams']} param., {len(f['code'])} instr.)")
    for pc, ins in enumerate(f["code"]):
        op = ins & 0x3f; c = (ins >> 6) & 0x1ff; b = (ins >> 15) & 0x1ff; a = ins >> 24; bx = (ins >> 6) & 0x3ffff; sbx = bx - 131071
        o = OPS[op] if op < len(OPS) else "?%d" % op
        if o in ("LOADK", "GETGLOBAL", "SETGLOBAL"): arg = f"r{a} {f['k'][bx]!r}"
        elif o in ("JMP",): arg = f"→ {pc + 1 + sbx}"
        elif o in ("FORLOOP", "TFORPREP"): arg = f"r{a} → {pc + 1 + sbx}"
        elif o == "CLOSURE": arg = f"r{a} fonction {bx}"
        elif o in ("EQ", "LT", "LE"): arg = f"{a} {rk(f, b)} {rk(f, c)}"
        elif o in ("ADD", "SUB", "MUL", "DIV", "POW", "GETTABLE", "SETTABLE", "SELF"): arg = f"r{a} {rk(f, b)} {rk(f, c)}"
        elif o == "LOADBOOL": arg = f"r{a} {bool(b)}"
        else: arg = f"r{a} {b} {c}"
        print(f"{pad}  {pc:4d} {o:9s} {arg}")
    for i, p in enumerate(f["protos"]):
        montrer(p, f"{nom}.{i}", prof + 1)


def lire(chemin, entree=None):
    if entree is None: return open(chemin, "rb").read()
    d = open(os.path.splitext(chemin)[0] + ".dir", "rb").read()
    for i in range(len(d) // 32):
        o, s = struct.unpack_from("<II", d, 32 * i)
        if d[32 * i + 8:32 * i + 32].split(b"\0")[0].decode().lower() == entree.lower():
            with open(chemin, "rb") as fh: fh.seek(o * 2048); return fh.read(s * 2048)
    sys.exit("entrée introuvable : " + entree)


if __name__ == "__main__":
    b = lire(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
    assert b[:5] == b"\x1bLuaP", "pas du Lua 5.0"
    # Après la version : endianness, int, size_t, instr, SIZE_OP/A/B/C, sizeof(Number),
    # puis le nombre test. Bully compile Lua avec des float 32 bits (sizeof = 4).
    r = R(b); r.taille_nombre = b[13]; r.p = 5 + 9 + r.taille_nombre
    montrer(fonction(r))
