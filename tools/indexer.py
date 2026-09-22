#!/usr/bin/env python3
"""Construit l'index du binaire : export/fonctions.jsonl (produit par
tools/ghidra/ExportTout.java) → export/index.sqlite, avec recherche plein
texte sur le pseudo-C. Ensuite tools/chercher.py répond en une seconde.

Usage : tools/indexer.py [export/fonctions.jsonl]
"""
import json, os, re, sqlite3, sys

racine = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(racine, "export/fonctions.jsonl")
dst = os.path.join(racine, "export/index.sqlite")
if os.path.exists(dst):
    os.remove(dst)
db = sqlite3.connect(dst)
db.executescript("""
CREATE TABLE fonctions (adresse TEXT PRIMARY KEY, nom TEXT, taille INT, code TEXT);
CREATE TABLE appels (de TEXT, vers TEXT);
CREATE TABLE chaines (adresse TEXT, chaine TEXT);
CREATE TABLE offsets (adresse TEXT, offset INT, acces TEXT);   -- +0x1d40 lu / écrit
CREATE TABLE globales (adresse TEXT, globale TEXT);
CREATE VIRTUAL TABLE code_fts USING fts5(adresse UNINDEXED, nom, code);
""")
re_off = re.compile(r"\(param_1 \+ (0x[0-9a-f]+)\)|\(this \+ (0x[0-9a-f]+)\)")
re_glob = re.compile(r"\b(DAT_[0-9a-f]{8}|PTR_[A-Za-z0-9_]+_[0-9a-f]{8})\b")
n = 0
with open(src, encoding="utf-8") as f:
    for ligne in f:
        r = json.loads(ligne)
        a = r["adresse"]
        db.execute("INSERT INTO fonctions VALUES (?,?,?,?)", (a, r["nom"], r["taille"], r["code"]))
        db.executemany("INSERT INTO appels VALUES (?,?)", [(a, v) for v in r["appels"]])
        db.executemany("INSERT INTO chaines VALUES (?,?)", [(a, s) for s in r["chaines"]])
        code = r["code"]
        vus = set()
        for m in re_off.finditer(code):
            off = int(m.group(1) or m.group(2), 16)
            # écriture si suivi de « = » hors « == »
            fin = m.end()
            suite = code[fin:fin + 6]
            acces = "w" if re.match(r"\)?\s*=[^=]", suite) else "r"
            if (off, acces) not in vus:
                vus.add((off, acces))
                db.execute("INSERT INTO offsets VALUES (?,?,?)", (a, off, acces))
        for g in set(re_glob.findall(code)):
            db.execute("INSERT INTO globales VALUES (?,?)", (a, g))
        db.execute("INSERT INTO code_fts VALUES (?,?,?)", (a, r["nom"], code))
        n += 1
db.executescript("""
CREATE INDEX i_appels_de ON appels(de); CREATE INDEX i_appels_vers ON appels(vers);
CREATE INDEX i_chaines ON chaines(chaine); CREATE INDEX i_offsets ON offsets(offset);
CREATE INDEX i_globales ON globales(globale);
""")
db.commit()
print(f"{n} fonctions indexées dans {dst}")
