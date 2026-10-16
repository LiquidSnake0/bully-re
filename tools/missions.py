#!/usr/bin/env python3
"""La table des missions, lue dans la banque d'arbres d'actions. Voir docs/missions.md.

    tools/missions.py            une ligne par mission : fichier, nœud, type, conditions, titre, script
    tools/missions.py --types    le compte par type (champ 40 de MissionData)

S'appuie sur tools/cat.py, qui sait lire les .cat : ce script ne fait que filtrer
les nœuds qui portent une piste MissionData.
"""
import os, re, subprocess, sys, collections

ICI = os.path.dirname(os.path.abspath(__file__))

# Les sous-arbres que Missions.cat rattache par référence, dans son ordre.
BANQUES = ["Missions", "chap1", "chap2", "chap3", "chap4", "chap5", "classes",
           "Minigames", "Punish", "Tutorials", "Debug"]

TYPES = {"0": "histoire", "1": "cours", "3": "boulot/course", "4": "tutoriel", "5": "mini-jeu"}


def missions():
    for banque in BANQUES:
        sortie = subprocess.run([sys.executable, os.path.join(ICI, "cat.py"), banque + ".cat", "--pistes"],
                                capture_output=True, text=True).stdout.split("\n")
        for i, ligne in enumerate(sortie):
            if "· MissionData" not in ligne:
                continue
            noeud = sortie[i - 1].strip()
            if not noeud.startswith("n "):
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
