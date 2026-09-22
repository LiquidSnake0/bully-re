// Exporte toutes les fonctions du programme en JSONL : adresse, nom,
// taille, appels sortants, appelants, chaines referencees, pseudo-C.
// C'est la matiere premiere de l'index (tools/indexer.py dans bully-re).
// Sortie : /work/export/fonctions.jsonl
//@category Bully
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import java.io.*;
import java.util.*;

public class ExportTout extends GhidraScript {
    private static String json(String s) {
        StringBuilder b = new StringBuilder("\"");
        for (char c : s.toCharArray()) {
            switch (c) {
                case '"': b.append("\\\""); break;
                case '\\': b.append("\\\\"); break;
                case '\n': b.append("\\n"); break;
                case '\r': break;
                case '\t': b.append("\\t"); break;
                default: if (c < 0x20) b.append(String.format("\\u%04x", (int) c)); else b.append(c);
            }
        }
        return b.append('"').toString();
    }

    @Override
    public void run() throws Exception {
        File dir = new File("/work/export");
        dir.mkdirs();
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        Listing listing = currentProgram.getListing();
        int n = 0;
        try (PrintWriter w = new PrintWriter(new BufferedWriter(new FileWriter(new File(dir, "fonctions.jsonl")), 1 << 20))) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Function f = it.next();
                List<String> appels = new ArrayList<>();
                for (Function c : f.getCalledFunctions(monitor)) appels.add(c.getEntryPoint().toString());
                List<String> appelants = new ArrayList<>();
                for (Function c : f.getCallingFunctions(monitor)) appelants.add(c.getEntryPoint().toString());
                // chaines referencees depuis le corps
                Set<String> chaines = new LinkedHashSet<>();
                InstructionIterator ii = listing.getInstructions(f.getBody(), true);
                while (ii.hasNext()) {
                    Instruction ins = ii.next();
                    for (Reference r : ins.getReferencesFrom()) {
                        Data data = listing.getDataAt(r.getToAddress());
                        if (data != null && data.hasStringValue()) {
                            Object v = data.getValue();
                            if (v != null) { String s = v.toString(); if (s.length() > 120) s = s.substring(0, 120); chaines.add(s); }
                        }
                    }
                }
                String code = "";
                DecompileResults r = d.decompileFunction(f, 30, monitor);
                if (r != null && r.decompileCompleted()) code = r.getDecompiledFunction().getC();
                StringBuilder sb = new StringBuilder();
                sb.append("{\"adresse\":").append(json(f.getEntryPoint().toString()))
                  .append(",\"nom\":").append(json(f.getName()))
                  .append(",\"taille\":").append(f.getBody().getNumAddresses())
                  .append(",\"appels\":[");
                for (int i = 0; i < appels.size(); i++) { if (i > 0) sb.append(','); sb.append(json(appels.get(i))); }
                sb.append("],\"appelants\":[");
                for (int i = 0; i < appelants.size(); i++) { if (i > 0) sb.append(','); sb.append(json(appelants.get(i))); }
                sb.append("],\"chaines\":[");
                int i = 0;
                for (String s : chaines) { if (i++ > 0) sb.append(','); sb.append(json(s)); }
                sb.append("],\"code\":").append(json(code)).append('}');
                w.println(sb);
                n++;
                if (n % 1000 == 0) println("ExportTout : " + n + " fonctions");
            }
        }
        d.dispose();
        println("ExportTout termine : " + n + " fonctions");
    }
}
