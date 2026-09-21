// Applique les noms de /work/fonctions-nommees.tsv (adresse, nom, source)
// aux fonctions du projet Ghidra, pour que les decompilations suivantes
// soient lisibles. Les noms a plusieurs candidats (avec |) sont ignores.
//@category Bully
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;
import java.util.List;

public class ApplyNames extends GhidraScript {
    @Override
    public void run() throws Exception {
        List<String> lignes = Files.readAllLines(Paths.get("/work/fonctions-nommees.tsv"));
        int n = 0;
        for (String l : lignes.subList(1, lignes.size())) {
            String[] c = l.split("\t");
            if (c.length < 2 || c[1].contains("|")) continue;
            Function f = getFunctionAt(toAddr(Long.parseLong(c[0], 16)));
            if (f == null) f = createFunction(toAddr(Long.parseLong(c[0], 16)), null);
            if (f == null) continue;
            String nom = c[1].replace("::", "__");
            f.setName(nom, SourceType.USER_DEFINED);
            n++;
        }
        println("ApplyNames : " + n + " fonctions renommees");
    }
}
