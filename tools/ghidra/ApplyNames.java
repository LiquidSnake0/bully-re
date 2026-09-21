// Applique les noms de /work/fonctions-nommees.tsv (adresse, nom, source)
// aux fonctions du projet Ghidra. Les adresses listees dans
// /work/fonctions-a-retirer.txt reprennent leur nom par defaut.
//@category Bully
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;
import java.util.List;

public class ApplyNames extends GhidraScript {
    @Override
    public void run() throws Exception {
        int n = 0, r = 0;
        Path retirer = Paths.get("/work/fonctions-a-retirer.txt");
        if (Files.exists(retirer)) {
            for (String l : Files.readAllLines(retirer)) {
                if (l.trim().isEmpty()) continue;
                Function f = getFunctionAt(toAddr(Long.parseLong(l.trim(), 16)));
                if (f != null) { f.getSymbol().setName("FUN_" + l.trim(), SourceType.DEFAULT); r++; }
            }
        }
        List<String> lignes = Files.readAllLines(Paths.get("/work/fonctions-nommees.tsv"));
        for (String l : lignes.subList(1, lignes.size())) {
            String[] c = l.split("\t");
            if (c.length < 2 || c[1].contains("|")) continue;
            Function f = getFunctionAt(toAddr(Long.parseLong(c[0], 16)));
            if (f == null) f = createFunction(toAddr(Long.parseLong(c[0], 16)), null);
            if (f == null) continue;
            f.setName(c[1].replace("::", "__"), SourceType.USER_DEFINED);
            n++;
        }
        println("ApplyNames : " + n + " fonctions nommees, " + r + " remises par defaut");
    }
}
