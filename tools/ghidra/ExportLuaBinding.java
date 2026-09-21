// Pour chaque nom de fonction Lua donne en argument : trouve la chaine dans
// le binaire, les tables luaL_Reg { const char *nom; lua_CFunction f; } qui
// la referencent, decompile f. C'est la passerelle entre les noms connus de
// l'API Lua de Bully et les fonctions du moteur qu'ils appellent.
// Sortie : /work/export/lua-<nom>.c
//@category Bully
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

public class ExportLuaBinding extends GhidraScript {
    @Override
    public void run() throws Exception {
        File dir = new File("/work/export");
        dir.mkdirs();
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        MemoryBlock text = currentProgram.getMemory().getBlock(".text");
        for (String nom : getScriptArgs()) {
            byte[] motif = (nom + "\0").getBytes("ASCII");
            Address chaine = currentProgram.getMemory().findBytes(currentProgram.getMinAddress(), motif, null, true, monitor);
            // il faut que le caractere precedent soit un 0 (debut de chaine)
            while (chaine != null && getByte(chaine.subtract(1)) != 0) {
                chaine = currentProgram.getMemory().findBytes(chaine.add(1), motif, null, true, monitor);
            }
            try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "lua-" + nom + ".c")))) {
                if (chaine == null) { w.println("// chaine introuvable : " + nom); println(nom + " : chaine introuvable"); continue; }
                w.println("// chaine " + nom + " @ " + chaine);
                int trouves = 0;
                ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(chaine);
                while (refs.hasNext()) {
                    Reference r = refs.next();
                    Address from = r.getFromAddress();
                    if (text.contains(from)) { w.println("// reference depuis du code @ " + from); continue; }
                    long ptr = getInt(from.add(4)) & 0xffffffffL;   // luaL_Reg : nom puis fonction
                    Address f = toAddr(ptr);
                    Function fn = getFunctionAt(f);
                    if (fn == null && text.contains(f)) fn = createFunction(f, "lua_" + nom);
                    w.println("// table @ " + from + " -> fonction " + f + (fn == null ? " (pas une fonction)" : ""));
                    if (fn == null) continue;
                    DecompileResults res = d.decompileFunction(fn, 60, monitor);
                    if (res != null && res.decompileCompleted()) { w.println(res.getDecompiledFunction().getC()); trouves++; }
                }
                println(nom + " : " + trouves + " fonction(s) decompilee(s)");
            }
        }
        d.dispose();
    }
}
