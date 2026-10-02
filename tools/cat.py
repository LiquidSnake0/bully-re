#!/usr/bin/env python3
"""Lecteur des arbres d'actions compressés (Act/Act.img, *.cat). Voir docs/cat.md.

  tools/cat.py <nom.cat> [--donnees]     l'arbre, noms résolus
  tools/cat.py --stats                   tous les fichiers : comptes vérifiés contre l'en-tête

Chargeur du jeu : FUN_005fb3f0 (« CompressedActionTree::load() »), analyse
FUN_005fb220 / FUN_005fb1a0 (banque) / FUN_005fa9c0 (nœud jouable) /
FUN_005fafb0 (enfants). Les noms sont des HashString (0x576d80) :
majuscules, h = h * 0x83 + c, 31 bits. Outil de lecture : la sortie n'entre
pas dans le dépôt (données du jeu).
"""
import struct, sys, os, re, glob

JEU = os.environ.get("BULLY_DATA", os.path.expanduser("~/Documents/bully-test/data/jeu"))
EXE = os.environ.get("BULLY_EXE", os.path.expanduser("~/Documents/bully-test/bully.exe"))


def hs(s):
    h = 0
    for c in s.upper().encode("latin-1"):
        h = (h * 0x83 + c) & 0xffffffff
    return h & 0x7fffffff


class Archive:
    def __init__(self, base):
        self.dir = open(os.path.join(JEU, base + ".dir"), "rb").read()
        self.img = open(os.path.join(JEU, base + ".img"), "rb")
        self.noms = [self.dir[32 * i + 8:32 * i + 32].split(b"\0")[0].decode("latin-1") for i in range(len(self.dir) // 32)]

    def lire(self, nom):
        for i, n in enumerate(self.noms):
            if n.lower() == nom.lower():
                o, s = struct.unpack_from("<II", self.dir, 32 * i)
                self.img.seek(o * 2048)
                return self.img.read(s * 2048)
        return None


def dictionnaire():
    """Hachage -> nom, à partir de toutes les chaînes plausibles."""
    mots = set()
    if os.path.exists(EXE):
        mots |= set(m.decode() for m in re.findall(rb"[A-Za-z_][A-Za-z0-9_ ]{1,60}", open(EXE, "rb").read()))
    act = Archive("Act/Act")
    for n in act.noms:
        mots.add(n.rsplit(".", 1)[0])
        b = act.lire(n)
        h = struct.unpack_from("<4I", b, 0)
        mots |= set(x.decode("latin-1") for x in b[h[2]:h[0]].split(b"\0") if x)
    for img in ("Stream/World", "Scripts/Scripts"):
        try:
            a = Archive(img)
            mots |= set(n.rsplit(".", 1)[0] for n in a.noms)
            if img.startswith("Scripts"):
                for n in a.noms:
                    mots |= set(m.decode() for m in re.findall(rb"[A-Za-z_][A-Za-z0-9_]{2,60}", a.lire(n)))
        except OSError:
            pass
    return {hs(m): m for m in mots}


class Cat:
    def __init__(self, b, noms):
        self.b, self.noms = b, noms
        self.h = struct.unpack_from("<8I", b, 0)   # taille, fin de l'en-tête, chaînes, fin de l'arbre, banques, jouables, références, feuilles
        self.cnt = {"b": 0, "r": 0, "n": 0, "l": 0}

    def chaine(self, off):
        o = self.h[2] + off
        return self.b[o:self.b.index(b"\0", o)].decode("latin-1")

    def nom_hache(self, v):
        return self.noms.get(v, "#%08x" % v)

    def nom(self, p):
        v = struct.unpack_from("<I", self.b, p)[0]
        return (self.nom_hache(v & 0x7fffffff) if v & 0x80000000 else self.chaine(v)), p + 4

    def pointeurs(self, p):
        n = self.b[p]
        return list(struct.unpack_from("<%di" % n, self.b, p + 1)), p + 1 + 4 * n

    def table(self, p):
        n = struct.unpack_from("<I", self.b, p)[0]; p += 4; t = []
        for _ in range(n):
            so, k = struct.unpack_from("<IH", self.b, p); p += 6
            t.append((so, list(struct.unpack_from("<%dI" % k, self.b, p)))); p += 4 * k
        return t, p

    def condition(self, off):
        """Un enregistrement de condition : son type (hachage du nom de classe)."""
        v = struct.unpack_from("<I", self.b, self.h[1] + off)[0]
        return self.nom_hache(v & 0x7fffffff)

    def noeud(self, p, genre, prof, sortie):
        nom, p = self.nom(p)
        conds, p = self.pointeurs(p)
        pistes = []
        if genre in "nl":
            pistes, p = self.pointeurs(p)
        txt = "  " * prof + "%s %s" % (genre, nom)
        if conds:
            txt += "  si " + " ".join(self.condition(c) for c in conds)
        if pistes:
            txt += "  [%d piste(s)]" % len(pistes)
        sortie.append(txt)
        return self.enfants(p, prof + 1, sortie)

    def enfants(self, p, prof, sortie):
        n = struct.unpack_from("<H", self.b, p)[0]; p += 2
        for _ in range(n):
            t = chr(self.b[p]); p += 1
            if t == "b":
                self.cnt["b"] += 1; p = self.noeud(p, "b", prof, sortie)
            elif t in "ir":
                self.cnt["r"] += 1
                a, c = struct.unpack_from("<ii", self.b, p); p += 8
                sortie.append("  " * prof + "%s %s -> %s" % (t, self.chaine(a), self.chaine(c)))
            elif t in "nl":
                self.cnt[t] += 1; p = self.noeud(p, t, prof, sortie)
            else:
                raise ValueError("type %r en %#x" % (t, p - 1))
        return p

    def arbre(self):
        self.renvois, p = self.table(0x20)        # chaînes : où les données les citent
        self.renvois2, p = self.table(p)          # seconde table, même forme
        if chr(self.b[p]) != "b":
            raise ValueError("racine attendue en %#x" % p)
        sortie = []
        fin = self.noeud(p + 1, "b", 0, sortie)
        return sortie, fin


def main():
    noms = dictionnaire()
    act = Archive("Act/Act")
    if sys.argv[1] == "--stats":
        ok = 0; resolus = total = 0
        for n in act.noms:
            c = Cat(act.lire(n), noms)
            try:
                sortie, fin = c.arbre()
            except Exception as e:
                print("%-28s échec : %s" % (n, e)); continue
            h = c.h
            bon = fin == h[3] and c.cnt["b"] == h[4] and c.cnt["r"] == h[6] and c.cnt["l"] == h[7] and c.cnt["n"] == h[5] - h[7]
            ok += bon
            for l in sortie:
                total += 1; resolus += "#" not in l.split()[1]
            if not bon:
                print("%-28s comptes %s, en-tête %s, fin %#x / %#x" % (n, c.cnt, h, fin, h[3]))
        print("%d / %d fichiers décodés et vérifiés ; noms de nœuds résolus : %d / %d" % (ok, len(act.noms), resolus, total))
        return
    c = Cat(act.lire(sys.argv[1]), noms)
    sortie, fin = c.arbre()
    print("en-tête", c.h, "fin de l'arbre %#x" % fin, c.cnt)
    print("\n".join(sortie))


if __name__ == "__main__":
    main()
