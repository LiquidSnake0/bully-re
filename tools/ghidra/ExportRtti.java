// Reconstruit la hierarchie des classes C++ de bully.exe a partir du RTTI
// MSVC (x86, pointeurs absolus) et mesure chaque table virtuelle.
// Sortie : /work/export/rtti.json
//@category Bully
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.*;

public class ExportRtti extends GhidraScript {

    private String lireNom(long typeDesc) throws Exception {
        // TypeDescriptor : +0 vftable, +4 spare, +8 nom decore ".?AVxxx@@"
        Address a = toAddr(typeDesc + 8);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 256; i++) {
            byte b = getByte(a.add(i));
            if (b == 0) break;
            sb.append((char) b);
        }
        String n = sb.toString();
        if (n.startsWith(".?AV") || n.startsWith(".?AU")) n = n.substring(4);
        if (n.endsWith("@@")) n = n.substring(0, n.length() - 2);
        // "Derive@Base@ns@" -> "ns::Base::Derive"
        String[] parts = n.split("@");
        List<String> l = Arrays.asList(parts);
        Collections.reverse(l);
        return String.join("::", l);
    }

    private long u32(long addr) throws Exception {
        return getInt(toAddr(addr)) & 0xffffffffL;
    }

    @Override
    public void run() throws Exception {
        MemoryBlock text = currentProgram.getMemory().getBlock(".text");
        long textDebut = text.getStart().getOffset(), textFin = text.getEnd().getOffset();
        Map<Long, String> symbolesVt = new HashMap<>();
        List<Address> vtables = new ArrayList<>();
        SymbolIterator si = currentProgram.getSymbolTable().getAllSymbols(true);
        while (si.hasNext()) {
            Symbol s = si.next();
            if (s.getName().equals("vftable")) {
                vtables.add(s.getAddress());
                symbolesVt.put(s.getAddress().getOffset(), s.getName(true));
            }
        }
        Collections.sort(vtables);
        File dir = new File("/work/export");
        dir.mkdirs();
        int ok = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "rtti.json")))) {
            w.println("[");
            boolean premier = true;
            for (int k = 0; k < vtables.size(); k++) {
                Address vt = vtables.get(k);
                try {
                    long col = u32(vt.getOffset() - 4);            // Complete Object Locator
                    long offset = u32(col + 4);                      // decalage de ce sous-objet
                    long td = u32(col + 12);                         // TypeDescriptor
                    long chd = u32(col + 16);                        // ClassHierarchyDescriptor
                    String nom = lireNom(td);
                    long nb = u32(chd + 8);                          // numBaseClasses (soi inclus)
                    long bca = u32(chd + 12);                        // BaseClassArray
                    List<String> bases = new ArrayList<>();
                    for (long i = 1; i < Math.min(nb, 64); i++) {    // 0 = la classe elle-meme
                        long bcd = u32(bca + 4 * i);
                        bases.add(lireNom(u32(bcd)));
                    }
                    // Taille de la vtable : pointeurs consecutifs vers .text,
                    // jusqu'a la vtable suivante.
                    long limite = k + 1 < vtables.size() ? vtables.get(k + 1).getOffset() - 4 : vt.getOffset() + 4096;
                    List<String> slots = new ArrayList<>();
                    for (long p = vt.getOffset(); p < limite; p += 4) {
                        long cible = u32(p);
                        if (cible < textDebut || cible > textFin) break;
                        Function f = getFunctionAt(toAddr(cible));
                        slots.add(f != null ? f.getName() : String.format("FUN_%08x?", cible));
                    }
                    if (!premier) w.println(",");
                    premier = false;
                    w.print("{\"nom\":\"" + nom + "\",\"vtable\":\"" + vt + "\",\"decalage\":" + offset
                        + ",\"bases\":[" + String.join(",", bases.stream().map(b -> "\"" + b + "\"").toList())
                        + "],\"slots\":[" + String.join(",", slots.stream().map(b -> "\"" + b + "\"").toList()) + "]}");
                    ok++;
                } catch (Exception e) {
                    println("vtable " + vt + " ignoree : " + e.getMessage());
                }
            }
            w.println("\n]");
        }
        println("ExportRtti : " + ok + " tables virtuelles decrites sur " + vtables.size());
    }
}
