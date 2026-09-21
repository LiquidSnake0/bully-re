#!/usr/bin/env python3
"""Genere docs/classes.md (arbre d'heritage) et src/squelettes/<classe>.h
(en-tetes vides avec les emplacements de la table virtuelle) a partir de
export/rtti.json produit par tools/ghidra/ExportRtti.java."""
import json, os, re, collections
racine = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
d = json.load(open(os.path.join(racine, "export/rtti.json")))
# une entree par classe : on garde le sous-objet principal (decalage 0)
classes = {}
for x in d:
    if x["decalage"] == 0 or x["nom"] not in classes:
        classes[x["nom"]] = x
enfants = collections.defaultdict(list)
parent = {}
for n, x in classes.items():
    p = x["bases"][0] if x["bases"] else None
    parent[n] = p
    enfants[p].append(n)
def arbre(n, prof, out):
    x = classes.get(n)
    slots = len(x["slots"]) if x else 0
    out.append(f"{'  ' * prof}- `{n}` ({slots} virtuelles)")
    for e in sorted(enfants[n]):
        arbre(e, prof + 1, out)
out = ["# Classes de bully.exe, reconstruites depuis le RTTI", "",
       f"{len(classes)} classes, {len(enfants[None])} racines. Le nombre entre parentheses est la taille de la table virtuelle.", ""]
for r in sorted(enfants[None]):
    arbre(r, 0, out)
open(os.path.join(racine, "docs/classes.md"), "w").write("\n".join(out) + "\n")
sq = os.path.join(racine, "src/squelettes")
os.makedirs(sq, exist_ok=True)
def ident(n): return re.sub(r"[^A-Za-z0-9_]", "_", n)
for n, x in classes.items():
    p = parent[n]
    lignes = [f"// Squelette genere depuis le RTTI de bully.exe. Table virtuelle @ {x['vtable']}.",
              f"#pragma once", ""]
    if p: lignes.append(f'#include "{ident(p)}.h"')
    lignes += ["", f"class {ident(n)}" + (f" : public {ident(p)}" if p else "") + " {", "public:"]
    for i, s in enumerate(x["slots"]):
        lignes.append(f"    // slot {i:2d} : {s}")
    lignes += ["};", ""]
    open(os.path.join(sq, ident(n) + ".h"), "w").write("\n".join(lignes))
print(len(classes), "classes ;", len(enfants[None]), "racines ;", len(os.listdir(sq)), "en-tetes")
