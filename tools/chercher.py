#!/usr/bin/env python3
"""Interroge export/index.sqlite.

  chercher.py offset 0x1d40          fonctions qui lisent / écrivent +0x1d40
  chercher.py globale DAT_00c1aea8   fonctions qui touchent une globale
  chercher.py chaine PEDSTATS        fonctions qui référencent une chaîne
  chercher.py appelle 00477480       qui appelle cette adresse
  chercher.py appels 00477480        ce que cette adresse appelle
  chercher.py texte "0x11c"          recherche plein texte dans le pseudo-C
  chercher.py code 00477480          affiche le pseudo-C
"""
import os, sqlite3, sys

racine = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
db = sqlite3.connect(os.path.join(racine, "export/index.sqlite"))
mode, arg = sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else ""

def nom(a):
    r = db.execute("SELECT nom, taille FROM fonctions WHERE adresse=?", (a,)).fetchone()
    return f"{a} {r[0]} ({r[1]} o)" if r else a

if mode == "offset":
    off = int(arg, 16) if arg.startswith("0x") else int(arg)
    for a, acces in db.execute("SELECT adresse, acces FROM offsets WHERE offset=? ORDER BY acces DESC, adresse", (off,)):
        print(acces, nom(a))
elif mode == "globale":
    for (a,) in db.execute("SELECT adresse FROM globales WHERE globale=? ORDER BY adresse", (arg,)):
        print(nom(a))
elif mode == "chaine":
    for a, s in db.execute("SELECT adresse, chaine FROM chaines WHERE chaine LIKE ? ORDER BY adresse", (f"%{arg}%",)):
        print(nom(a), "→", s)
elif mode == "appelle":
    for (a,) in db.execute("SELECT de FROM appels WHERE vers=? ORDER BY de", (arg,)):
        print(nom(a))
elif mode == "appels":
    for (a,) in db.execute("SELECT vers FROM appels WHERE de=? ORDER BY vers", (arg,)):
        print(nom(a))
elif mode == "texte":
    for a, n in db.execute("SELECT adresse, nom FROM code_fts WHERE code_fts MATCH ? LIMIT 60", (f'"{arg}"',)):
        print(nom(a))
elif mode == "code":
    r = db.execute("SELECT code FROM fonctions WHERE adresse=?", (arg,)).fetchone()
    print(r[0] if r else "inconnue")
else:
    print(__doc__)
