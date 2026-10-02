#!/usr/bin/env python3
"""Lecteur des arbres d'actions compressés (Act/Act.img, *.cat). Voir docs/cat.md.

  tools/cat.py <nom.cat> [--pistes]      l'arbre, noms résolus ; --pistes : les pistes de chaque nœud
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


def rattachements():
    """HashString("GROUPE\\NOM") -> (groupe, indice dans le .agr), d'après les .HXD (docs/hxd.md)."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from hxd import lire
    r = {}
    for f in glob.glob(os.path.join(JEU, "Anim", "*.HXD")):
        d = lire(open(f, "rb").read())
        compte = {}
        for a in d["anims"]:
            g = a["f"]; k = compte.get(g, 0); compte[g] = k + 1
            r[a["c"]] = (d["t3"][g][0], k)
    return r


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
    # Les animations : « GROUPE\NOM » dans les .HXD et hxds.dat d'Anim (MAINPED.HXD…).
    for f in glob.glob(os.path.join(JEU, "Anim", "*.HXD")) + glob.glob(os.path.join(JEU, "Anim", "hxds.dat")):
        mots |= set(m.decode("latin-1") for m in re.findall(rb"[A-Za-z0-9_]{2,40}\\[A-Za-z0-9_ ]{1,60}", open(f, "rb").read()))
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

    def champs(self, at, prof=0):
        """Les attributs d'une piste, héritage compris : position -> (octets, décalage dans les données)."""
        base = struct.unpack_from("<H", self.b, at)[0]
        res = self.champs(at + base, prof + 1) if base and prof < 32 else {}
        q = at + 2
        while True:
            w = struct.unpack_from("<H", self.b, q)[0]; q += 2
            t = 1 << ((w >> 1) & 3)
            res[w >> 3] = (self.b[q:q + t], q - self.h[1]); q += t
            if not w & 1:
                return res

    def valeur(self, v, dec):
        cite = getattr(self, "cites", {})
        if dec in cite:
            return repr(cite[dec])
        if len(v) == 4:
            x = struct.unpack("<I", v)[0]; f = struct.unpack("<f", v)[0]
            if x > 0xffff and (x & 0x7fffffff) in self.noms:
                ra = getattr(self, "ratt", {}).get(x & 0x7fffffff)
                return self.noms[x & 0x7fffffff] + (" [%s.agr n° %d]" % ra if ra else "")
            if 1e-3 < abs(f) < 1e5:
                return "%.3g" % f
            return str(x) if x < 0x10000 else "%#x" % x
        return v.hex()

    def piste(self, off):
        ch = self.champs(self.h[1] + off)
        t = struct.unpack("<I", ch[0][0])[0] & 0x7fffffff if 0 in ch else 0
        return self.nom_hache(t) + " " + ", ".join("%d=%s" % (k, self.valeur(v, d)) for k, (v, d) in sorted(ch.items()) if k)

    def noeud(self, p, genre, prof, sortie):
        nom, p = self.nom(p)
        conds, p = self.pointeurs(p)
        pistes = []
        if genre in "nl":
            pistes, p = self.pointeurs(p)
        txt = "  " * prof + "%s %s" % (genre, nom)
        if conds:
            txt += "  si " + " ".join(self.condition(c) for c in conds)
        if pistes and not getattr(self, "voir_pistes", False):
            txt += "  [%d piste(s)]" % len(pistes)
        sortie.append(txt)
        if getattr(self, "voir_pistes", False):
            for t in pistes:
                sortie.append("  " * prof + "   · " + self.piste(t))
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
    c.voir_pistes = "--pistes" in sys.argv
    if c.voir_pistes:
        c.ratt = rattachements()
        c.renvois, _ = c.table(0x20)
        c.cites = {d: c.chaine(v) for v, ds in c.renvois for d in ds}
    sortie, fin = c.arbre()
    print("en-tête", c.h, "fin de l'arbre %#x" % fin, c.cnt)
    print("\n".join(sortie))


if __name__ == "__main__":
    main()
