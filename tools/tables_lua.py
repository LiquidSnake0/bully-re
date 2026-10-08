#!/usr/bin/env python3
"""Toutes les fonctions C que bully.exe enregistre dans Lua, lues dans le binaire.

  tools/tables_lua.py <bully.exe> > docs/api-lua-tables.tsv

Le moteur range ses liaisons dans des tables { const char *nom, lua_CFunction f }
finies par { 0, 0 } (le luaL_reg de Lua 5.0). On parcourt .rdata et .data mot par
mot : une table est une suite d'au moins trois paires (pointeur vers un nom ASCII,
pointeur dans .text). Les cinq tables de .rdata sont les bibliothèques standard de
Lua (base, coroutine, debug, math, table) ; les 55 de .data sont l'API du moteur.
Sortie : nom, fonction C, table (section:adresse), triée par nom.
"""
import struct, sys

b = open(sys.argv[1], 'rb').read()
pe = struct.unpack_from('<I', b, 0x3c)[0]
nsec = struct.unpack_from('<H', b, pe + 6)[0]
optsz = struct.unpack_from('<H', b, pe + 20)[0]
base = struct.unpack_from('<I', b, pe + 24 + 28)[0]
secs = []                                   # (nom, adresse virtuelle, taille virtuelle, décalage fichier, taille fichier)
for i in range(nsec):
    o = pe + 24 + optsz + i * 40
    nom = b[o:o + 8].split(b'\0')[0].decode()
    vsz, va, rsz, raw = struct.unpack_from('<IIII', b, o + 8)
    secs.append((nom, base + va, vsz, raw, rsz))
text = next(s for s in secs if s[0] == '.text')

def section(v):
    for s in secs:
        if s[1] <= v < s[1] + s[4]:
            return s

def nom_ascii(v):
    s = section(v)
    if s is None or s[0] == '.text':
        return None
    o = s[3] + v - s[1]
    t = b[o:o + 64].split(b'\0')[0]
    if not 2 <= len(t) <= 60 or not all(c < 128 and (chr(c).isalnum() or c == 95) for c in t):
        return None
    return t.decode()

noms = {}
for s in secs:
    if s[0] not in ('.rdata', '.data'):
        continue
    d = b[s[3]:s[3] + s[4]]
    i = 0
    while i + 8 <= len(d):
        suite, j = [], i
        while j + 8 <= len(d):
            p, f = struct.unpack_from('<II', d, j)
            n = nom_ascii(p)
            if n is None or not text[1] <= f < text[1] + text[2]:
                break
            suite.append((n, f))
            j += 8
        if len(suite) >= 3 and struct.unpack_from('<II', d, j) == (0, 0):
            for n, f in suite:
                noms.setdefault(n, (f, '%s:%08x' % (s[0], s[1] + i)))
            i = j
        else:
            i += 4

print('nom\tfonction_c\ttable')
for n in sorted(noms, key=str.lower):
    print('%s\t%08x\t%s' % (n, noms[n][0], noms[n][1]))
