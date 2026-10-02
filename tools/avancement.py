#!/usr/bin/env python3
"""Compteur d'avancement : combien des fonctions de bully.exe le dépôt a recréées.

  tools/avancement.py            écrit docs/avancement.md
  tools/avancement.py --court    une ligne, pour le terminal

La liste des fonctions (adresse, taille) vient de l'export Ghidra,
bully-test/export/fonctions.tsv (variable BULLY_FONCTIONS) ; elle reste hors du
dépôt. Une fonction compte :
  - « recréée » si son adresse (0x… ou FUN_…) est citée dans src/ (hors
    src/squelettes, générés depuis le RTTI) : le code qui la refait ou qui
    l'appelle sous son vrai nom ;
  - « comprise » si elle est recréée, ou citée dans docs/, outils/, tools/, ou
    nommée dans docs/fonctions-nommees.tsv.
Une adresse qui n'est pas un début de fonction (une donnée, une constante) ne
compte pas. Le pourcentage pondéré par la taille dit mieux l'effort : une
fonction de 20 octets ne vaut pas une boucle de jeu de 5 000.
"""
import glob, os, re, sys, datetime

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONCTIONS = os.environ.get("BULLY_FONCTIONS", os.path.expanduser("~/Documents/bully-test/export/fonctions.tsv"))
MOTIF = re.compile(r"(?:FUN_|0x)([0-9a-fA-F]{6,8})\b")


def fonctions():
    f = {}
    with open(FONCTIONS) as fh:
        next(fh)
        for l in fh:
            p = l.rstrip("\n").split("\t")
            if len(p) >= 2:
                f[int(p[0], 16)] = int(p[1])
    return f


def citees(chemins, f):
    par_fichier, toutes = {}, set()
    for c in chemins:
        s = {int(m.group(1), 16) for m in MOTIF.finditer(open(c, errors="ignore").read())} & f.keys()
        if s:
            par_fichier[os.path.relpath(c, RACINE)] = s
            toutes |= s
    return toutes, par_fichier


def main():
    os.chdir(RACINE)
    if not os.path.exists(FONCTIONS):
        sys.exit("liste des fonctions introuvable : " + FONCTIONS + " (BULLY_FONCTIONS)")
    f = fonctions()
    total_n, total_o = len(f), sum(f.values())
    src = [c for c in glob.glob("src/**/*", recursive=True) if c.endswith((".cpp", ".h")) and "/squelettes/" not in c]
    rec, par_fichier = citees(src, f)
    doc, _ = citees(glob.glob("docs/*") + glob.glob("outils/*") + glob.glob("tools/*.py"), f)
    nom = set()
    with open("docs/fonctions-nommees.tsv") as fh:
        next(fh)
        nom = {int(l.split("\t")[0], 16) for l in fh if l.strip()} & f.keys()
    comp = rec | doc | nom
    o = lambda s: sum(f[a] for a in s)
    pc = lambda a, b: 100.0 * a / b
    v = lambda x: ("%.1f" % x).replace(".", ",")
    e = lambda n: "{:,}".format(n).replace(",", "\u202f")
    if "--court" in sys.argv:
        print("bully-re : %d / %s fonctions recréées (%s %%, %s %% du code) ; %d comprises (%s %% du code)"
              % (len(rec), e(total_n), v(pc(len(rec), total_n)), v(pc(o(rec), total_o)), len(comp), v(pc(o(comp), total_o))))
        return
    sous = {}
    for c, s in par_fichier.items():
        d = c.split("/")[1] if c.count("/") > 1 else c[4:]
        sous.setdefault(d, set()).update(s)
    lignes = [
        "# Avancement",
        "",
        "Combien des fonctions de `bully.exe` le dépôt a recréées. Généré par",
        "`tools/avancement.py` (mis à jour à chaque commit) ; méthode dans l'en-tête du script.",
        "",
        "| | Fonctions | % du nombre | Octets de code | % du code |",
        "|---|---:|---:|---:|---:|",
        "| **Recréées** (citées dans `src/`) | %s | %s %% | %s | **%s %%** |" % (e(len(rec)), v(pc(len(rec), total_n)), e(o(rec)), v(pc(o(rec), total_o))),
        "| Comprises (code, docs, outils, noms) | %s | %s %% | %s | %s %% |" % (e(len(comp)), v(pc(len(comp), total_n)), e(o(comp)), v(pc(o(comp), total_o))),
        "| Total de l'exe | %s | 100 %% | %s | 100 %% |" % (e(total_n), e(total_o)),
        "",
        "## Par sous-système (fonctions citées dans `src/`)",
        "",
        "| Dossier | Fonctions | Octets |",
        "|---|---:|---:|",
    ]
    for d, s in sorted(sous.items(), key=lambda x: -o(x[1])):
        lignes.append("| `src/%s` | %d | %s |" % (d, len(s), e(o(s))))
    lignes += ["", "Relevé du %s." % datetime.date.today().strftime("%d.%m.%Y"), ""]
    open("docs/avancement.md", "w").write("\n".join(lignes))
    print("docs/avancement.md : %d recréées (%s %% du code), %d comprises" % (len(rec), v(pc(o(rec), total_o)), len(comp)))


if __name__ == "__main__":
    main()
