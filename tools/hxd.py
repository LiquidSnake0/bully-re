#!/usr/bin/env python3
"""Lecteur des dictionnaires d'animations .HXD (docs/hxd.md). Usage : tools/hxd.py <fichier.HXD>
Outil de lecture : la sortie n'entre pas dans le dépôt (données du jeu)."""
import struct,sys
class R:
    def __init__(s,b,p=0): s.b=b; s.p=p
    def u32(s): v=struct.unpack_from("<I",s.b,s.p)[0]; s.p+=4; return v
    def f32(s): v=struct.unpack_from("<f",s.b,s.p)[0]; s.p+=4; return v
    def u16(s): v=struct.unpack_from("<H",s.b,s.p)[0]; s.p+=2; return v
    def raw(s,n): v=s.b[s.p:s.p+n]; s.p+=n; return v
    def nom(s,n): v=s.raw(n); return v.split(b"\0")[0].decode('latin-1')
def lire(b,p=0):
    r=R(b,p); h={}
    h['version']=r.f32(); h['mot6']=r.u32()
    nm=r.u32(); masques=[]
    for _ in range(nm):
        nom=r.nom(0x20); n=r.u32(); x=r.u32(); poids=[r.f32() for _ in range(n)]
        masques.append((nom,n,x))
    nos=r.u32(); os_=[r.nom(0x20) for _ in range(nos)]
    na=r.u32(); anims=[]
    v=h['version']
    for i in range(na):
        a=r.u32(); b2=r.u32(); nom=r.nom(0x40); c=r.u32(); d=r.u32(); e=r.u32(); f=r.u32()
        nev=r.u16(); evs=[]
        for _ in range(nev):
            x,y,t=r.u32(),r.u32(),r.u32()
            if t==1: r.raw(8); r.raw(0x20)
            evs.append((x,y,t))
        g=r.u16(); hh=r.u16(); ii=r.u16(); jj=r.u16(); r.raw(4)
        if v>1.09+1e-6:
            r.raw(4)
            if v>1.11+1e-6: r.raw(4)
        vec=struct.unpack_from("<3f",r.b,r.p); r.p+=12
        anims.append(dict(i=i,nom=nom,a=a,b=b2,c=c,d=d,e=e,f=f,ev=len(evs),g=g,h=hh,ii=ii,jj=jj))
    nt=r.u32(); t3=[]
    for _ in range(nt):
        nom=r.nom(0x20); x=r.u32()
        if not (v<1.11+1e-6 and abs(v-1.11)>1e-6): r.raw(0x20); x2=r.u32()
        t3.append((nom,x))
    r.raw(nos*12)
    n4=r.u32(); r.raw(n4*0x30)
    fin=r.u32()
    return dict(h=h,masques=masques,os=os_,anims=anims,t3=t3,n4=n4,fin=fin,p=r.p)
if __name__=="__main__":
    b=open(sys.argv[1],"rb").read(); d=lire(b)
    print("taille",len(b),"lu jusqu'à",d['p'],d['h'],"masques",len(d['masques']),"os",len(d['os']),"anims",len(d['anims']),"t3",len(d['t3']),"n4",d['n4'])
    for a in d['anims'][:8]: print(a)
    print(d['t3'][:5])
