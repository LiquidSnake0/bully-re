// Script Ghidra headless : inventaire des fonctions de bully.exe et
// decompilation d'un echantillon, pour mesurer ce que vaut un test de
// recreation de moteur. Ecrit dans /work/export/.
//
// Arguments : [motif de nom] [nombre max de fonctions a decompiler]
//@category Bully
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.SymbolIterator;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

public class ExportBully extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String motif = args.length > 0 ? args[0] : "";
        int maxDecomp = args.length > 1 ? Integer.parseInt(args[1]) : 0;
        File dir = new File("/work/export");
        dir.mkdirs();

        // 1. Inventaire des fonctions : adresse, taille, nom, nb d'appels sortants.
        int total = 0, nommees = 0;
        long octets = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "fonctions.tsv")))) {
            w.println("adresse\ttaille\tnom\tappels_sortants");
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext()) {
                Function f = it.next();
                long taille = f.getBody().getNumAddresses();
                int appels = f.getCalledFunctions(monitor).size();
                w.println(f.getEntryPoint() + "\t" + taille + "\t" + f.getName() + "\t" + appels);
                total++;
                octets += taille;
                if (!f.getName().startsWith("FUN_")) nommees++;
            }
        }

        // 2. Classes vues par le RTTI MSVC (symboles vftable / RTTI).
        int classes = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "classes.txt")))) {
            SymbolTable st = currentProgram.getSymbolTable();
            SymbolIterator si = st.getAllSymbols(true);
            while (si.hasNext()) {
                Symbol s = si.next();
                String n = s.getName();
                if (n.contains("vftable") || n.startsWith("RTTI_Type_Descriptor")) {
                    w.println(s.getAddress() + "\t" + s.getName(true));
                    classes++;
                }
            }
        }

        // 3. Decompilation d'un echantillon.
        int decomp = 0;
        if (maxDecomp > 0) {
            Pattern p = Pattern.compile(motif);
            DecompInterface d = new DecompInterface();
            d.openProgram(currentProgram);
            List<Function> cibles = new ArrayList<>();
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext() && cibles.size() < maxDecomp) {
                Function f = it.next();
                if (p.matcher(f.getName(true)).find()) cibles.add(f);
            }
            try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "echantillon.c")))) {
                for (Function f : cibles) {
                    DecompileResults r = d.decompileFunction(f, 60, monitor);
                    w.println("// ===== " + f.getName(true) + " @ " + f.getEntryPoint()
                        + " (" + f.getBody().getNumAddresses() + " octets)");
                    if (r != null && r.decompileCompleted()) {
                        w.println(r.getDecompiledFunction().getC());
                        decomp++;
                    } else {
                        w.println("// echec de decompilation");
                    }
                }
            }
            d.dispose();
        }

        try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "resume.txt")))) {
            w.println("fonctions : " + total);
            w.println("fonctions nommees (RTTI, imports, chaines) : " + nommees);
            w.println("octets de code dans des fonctions : " + octets);
            w.println("symboles de classes (vftable/RTTI) : " + classes);
            w.println("fonctions decompilees dans l'echantillon : " + decomp);
        }
        println("ExportBully termine : " + total + " fonctions, " + classes + " symboles de classes.");
    }
}
