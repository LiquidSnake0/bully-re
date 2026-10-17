#!/usr/bin/env python3
"""La table des missions, lue dans la banque d'arbres d'actions. Voir docs/missions.md.

    tools/missions.py            une ligne par mission : fichier, nœud, type, conditions, titre, script
    tools/missions.py --types    le compte par type (champ 40 de MissionData)

S'appuie sur tools/cat.py, qui sait lire les .cat : ce script ne fait que filtrer
les nœuds qui portent une piste MissionData.
"""
import os, re, subprocess, sys, collections

ICI = os.path.dirname(os.path.abspath(__file__))

# Missions.cat rattache onze sous-arbres par référence (lignes « r chap1.act -> … ») : on les suit
# dans l'ordre du fichier, comme FUN_006ac5d0, Doors compris.
RACINE = "Missions"

TYPES = {"0": "histoire", "1": "cours", "3": "boulot/course", "4": "tutoriel", "5": "mini-jeu"}


def lire(banque):
    return subprocess.run([sys.executable, os.path.join(ICI, "cat.py"), banque + ".cat", "--pistes"],
                          capture_output=True, text=True).stdout.split("\n")


def missions():
    banques = [RACINE]
    for ligne in lire(RACINE):
        m = re.match(r"\s*r (\S+)\.act ", ligne)
        if m:
            banques.append(m.group(1))
    for banque in banques:
        sortie = lire(banque)
        noeud = ""
        for ligne in sortie:
            # Le dernier nœud ouvert : une piste MissionData peut suivre une autre piste du même nœud.
            d = ligne.strip()
            if d[:2] in ("n ", "b ", "l "):
                noeud = d
            if "· MissionData" not in ligne:
                continue
            nom, _, condition = noeud[2:].partition("  si ")
            champs = dict(re.findall(r"(\d+)=('(?:[^']*)'|[-\w.]+)", ligne))
            yield {
                "banque": banque,
                "noeud": nom.strip(),
                "conditions": condition.strip() or "-",
                "type": champs.get("40", "?"),
                "titre": champs.get("24", "").strip("'"),
                "script": champs.get("28", "").strip("'").replace("\\\\", "\\"),
            }


def main():
    liste = list(missions())
    if "--types" in sys.argv:
        compte = collections.Counter(m["type"] for m in liste)
        for t, n in sorted(compte.items()):
            print(f"{t}\t{TYPES.get(t, '?')}\t{n}")
        print(f"total\t\t{len(liste)}")
        return
    for m in liste:
        print("\t".join([m["banque"], m["noeud"], m["type"], m["conditions"], m["titre"], m["script"]]))


if __name__ == "__main__":
    main()
