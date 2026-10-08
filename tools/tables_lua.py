#!/usr/bin/env python3
"""Toutes les fonctions C que bully.exe enregistre dans Lua, lues dans le binaire.

  tools/tables_lua.py <bully.exe> > docs/api-lua-tables.tsv

CreateLuaState (0x5db260, lu avec REA le 08.10.2026) ouvre l'état Lua, les cinq
bibliothèques standard (base, table, math, debug, coroutine dans base), puis
enregistre l'API du moteur table par table :
    push <table> ; mov ecx, esi ; call 0x5dafe0
Chaque table est un luaL_reg de Lua 5.0 : paires { const char *nom, lua_CFunction f }
finies par { 0, 0 }. On relit ces appels dans le code, puis chaque table.
Sortie : nom, fonction C, table, triée par nom ; une ligne de commentaire par table
(dans l'ordre d'enregistrement) sur la sortie d'erreur.
"""
import struct, sys

CREATE_LUA_STATE = (0x5db260, 0x5db6cf)
ENREGISTRER_TABLE = 0x5dafe0

b = open(sys.argv[1], 'rb').read()
pe = struct.unpack_from('<I', b, 0x3c)[0]
nsec = struct.unpack_from('<H', b, pe + 6)[0]
optsz = struct.unpack_from('<H', b, pe + 20)[0]
base = struct.unpack_from('<I', b, pe + 24 + 28)[0]
secs = []                                   # (nom, adresse virtuelle, décalage fichier, taille fichier)
for i in range(nsec):
    o = pe + 24 + optsz + i * 40
    vsz, va, rsz, raw = struct.unpack_from('<IIII', b, o + 8)
    secs.append((b[o:o + 8].split(b'\0')[0].decode(), base + va, raw, rsz))

def fichier(v):
    for nom, va, raw, rsz in secs:
        if va <= v < va + rsz:
            return raw + v - va
    raise ValueError('adresse hors du fichier : 0x%x' % v)

def chaine(v):
    o = fichier(v)
    return b[o:b.index(b'\0', o)].decode('latin-1')

debut, fin = CREATE_LUA_STATE
code = b[fichier(debut):fichier(fin)]
tables = []
for i in range(len(code) - 12):
    # 68 imm32 (push) ; 8b ce (mov ecx, esi) ; e8 rel32 (call)
    if code[i] == 0x68 and code[i + 5:i + 7] == b'\x8b\xce' and code[i + 7] == 0xe8:
        cible = debut + i + 12 + struct.unpack_from('<i', code, i + 8)[0]
        if cible == ENREGISTRER_TABLE:
            tables.append(struct.unpack_from('<I', code, i + 1)[0])

noms = {}
for t in tables:
    k, n = t, 0
    while True:
        p, f = struct.unpack_from('<II', b, fichier(k))
        if p == 0 and f == 0:
            break
        noms.setdefault(chaine(p), (f, t))
        k += 8
        n += 1
    print('# table 0x%08x : %d fonctions (première : %s)' % (t, n, chaine(struct.unpack_from('<I', b, fichier(t))[0])), file=sys.stderr)
print('# %d tables, %d fonctions' % (len(tables), len(noms)), file=sys.stderr)

print('nom\tfonction_c\ttable')
for n in sorted(noms, key=str.lower):
    print('%s\t%08x\t%08x' % (n, noms[n][0], noms[n][1]))
