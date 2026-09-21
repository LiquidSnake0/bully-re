// Decompile les methodes virtuelles d'une classe a partir de sa vftable
// (RTTI MSVC), pour obtenir un echantillon de code du jeu et non du moteur.
// Arguments : <nom de classe> [nombre max de methodes]
//@category Bully
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

public class ExportVtable extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String classe = args[0];
        int max = args.length > 1 ? Integer.parseInt(args[1]) : 20;
        File dir = new File("/work/export");
        dir.mkdirs();
        Address vt = null;
        SymbolIterator si = currentProgram.getSymbolTable().getAllSymbols(true);
        while (si.hasNext()) {
            Symbol s = si.next();
            String n = s.getName(true);
            if (n.endsWith(classe + "::vftable")) { vt = s.getAddress(); println("vftable " + n + " @ " + vt); break; }
        }
        if (vt == null) { println("vftable introuvable pour " + classe); return; }
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        int n = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "vtable-" + classe + ".c")))) {
            w.println("// Methodes virtuelles de " + classe + ", vftable @ " + vt);
            for (int i = 0; i < max; i++) {
                Address slot = vt.add(4L * i);
                long ptr = getInt(slot) & 0xffffffffL;
                Address cible = toAddr(ptr);
                Function f = getFunctionAt(cible);
                if (f == null) { w.println("// slot " + i + " -> " + cible + " : pas une fonction, fin"); break; }
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                w.println("\n// ===== slot " + i + " : " + f.getName() + " @ " + f.getEntryPoint()
                    + " (" + f.getBody().getNumAddresses() + " octets)");
                if (r != null && r.decompileCompleted()) { w.println(r.getDecompiledFunction().getC()); n++; }
                else w.println("// echec de decompilation");
            }
        }
        d.dispose();
        println("ExportVtable : " + n + " methodes decompilees pour " + classe);
    }
}
