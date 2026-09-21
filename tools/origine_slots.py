#!/usr/bin/env python3
"""Pour une classe, dit pour chaque emplacement de sa table virtuelle quelle
classe l'introduit et laquelle fournit l'implémentation en table.

Usage : tools/origine_slots.py CPlayerPed
"""
import json, os, sys
racine = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
d = json.load(open(os.path.join(racine, "export/rtti.json")))
cl = {x["nom"]: x for x in d if x["decalage"] == 0}
nom = sys.argv[1]
x = cl[nom]
chaine = list(reversed(x["bases"])) + [nom]      # de la racine vers la classe
chaine = [c for c in chaine if c in cl]
print(" → ".join(f"{c} ({len(cl[c]['slots'])})" for c in chaine))
for i, s in enumerate(x["slots"]):
    intro = next(c for c in chaine if i < len(cl[c]["slots"]))
    impl = next((c for c in chaine if i < len(cl[c]["slots"]) and cl[c]["slots"][i] == s), None)
    print(f"{i:2d} {s:22s} introduit par {intro:14s} implémenté par {impl or nom + ' (propre)'}")
