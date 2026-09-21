// Carte complete de l'API Lua : pour chaque nom lu dans /work/lua-api.txt,
// la fonction C enregistree et les fonctions du moteur qu'elle appelle.
// Sortie : /work/export/api-lua-map.tsv (nom, fonction C, taille, appels).
//@category Bully
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.io.*;
import java.nio.file.*;
import java.util.*;

public class ExportLuaMap extends GhidraScript {
    @Override
    public void run() throws Exception {
        List<String> noms = Files.readAllLines(Paths.get("/work/lua-api.txt"));
        MemoryBlock text = currentProgram.getMemory().getBlock(".text");
        File dir = new File("/work/export");
        dir.mkdirs();
        int ok = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "api-lua-map.tsv")))) {
            w.println("nom\tfonction_c\ttaille\tappels");
            for (String nom : noms) {
                nom = nom.trim();
                if (nom.isEmpty()) continue;
                byte[] motif = (nom + "\0").getBytes("ASCII");
                Address chaine = currentProgram.getMemory().findBytes(currentProgram.getMinAddress(), motif, null, true, monitor);
                while (chaine != null && getByte(chaine.subtract(1)) != 0)
                    chaine = currentProgram.getMemory().findBytes(chaine.add(1), motif, null, true, monitor);
                if (chaine == null) { w.println(nom + "\t\t\tchaine introuvable"); continue; }
                Function fn = null;
                ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(chaine);
                while (refs.hasNext() && fn == null) {
                    Address from = refs.next().getFromAddress();
                    if (text.contains(from)) continue;
                    Address f = toAddr(getInt(from.add(4)) & 0xffffffffL);
                    if (!text.contains(f)) continue;
                    fn = getFunctionAt(f);
                    if (fn == null) fn = createFunction(f, "lua_" + nom);
                }
                if (fn == null) { w.println(nom + "\t\t\tpas de table"); continue; }
                List<String> appels = new ArrayList<>();
                for (Function c : fn.getCalledFunctions(monitor)) appels.add(c.getName() + "@" + c.getEntryPoint());
                Collections.sort(appels);
                w.println(nom + "\t" + fn.getEntryPoint() + "\t" + fn.getBody().getNumAddresses() + "\t" + String.join(" ", appels));
                ok++;
            }
        }
        println("ExportLuaMap : " + ok + " liaisons sur " + noms.size());
    }
}
