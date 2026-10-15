// Dumps per-function statistics as TSV for scoping the reverse-engineering effort.
//@category thod2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import java.io.*;
import java.util.*;

public class ScopeFuncStats extends GhidraScript {
  @Override
  protected void run() throws Exception {
    String out = getScriptArgs()[0];
    Listing listing = currentProgram.getListing();
    ReferenceManager rm = currentProgram.getReferenceManager();
    FunctionManager fm = currentProgram.getFunctionManager();
    try (PrintWriter w = new PrintWriter(new OutputStreamWriter(new FileOutputStream(out), "UTF-8"))) {
      w.println("entry\tname\tsource\tthunk\tbytes\tinsns\tcallers\tcallees\timports\tstrings\tfloat_insns");
      for (Function f : fm.getFunctions(true)) {
        AddressSetView body = f.getBody();
        int insns = 0, flt = 0;
        Set<String> imports = new TreeSet<>();
        List<String> strs = new ArrayList<>();
        for (Instruction ins : listing.getInstructions(body, true)) {
          insns++;
          String m = ins.getMnemonicString();
          if (m.startsWith("F") || m.endsWith("SS") || m.endsWith("PS")) flt++;
          for (Reference r : ins.getReferencesFrom()) {
            Address to = r.getToAddress();
            Symbol s = currentProgram.getSymbolTable().getPrimarySymbol(to);
            if (s != null && s.isExternal()) imports.add(s.getName());
            Data d = listing.getDataAt(to);
            if (d != null) {
              if (d.isPointer()) {
                Object v = d.getValue();
                if (v instanceof Address) {
                  Symbol es = currentProgram.getSymbolTable().getPrimarySymbol((Address) v);
                  if (es != null && es.isExternal()) imports.add(es.getName());
                }
              }
              if (d.hasStringValue() && strs.size() < 6) {
                String sv = String.valueOf(d.getValue()).replaceAll("[\\t\\r\\n|]", " ");
                if (sv.length() > 40) sv = sv.substring(0, 40);
                strs.add(sv);
              }
            }
          }
        }
        int callers = f.getCallingFunctions(monitor).size();
        int callees = f.getCalledFunctions(monitor).size();
        w.println(f.getEntryPoint() + "\t" + f.getName() + "\t" + f.getSymbol().getSource() + "\t" + f.isThunk()
          + "\t" + body.getNumAddresses() + "\t" + insns + "\t" + callers + "\t" + callees
          + "\t" + String.join(",", imports) + "\t" + String.join("|", strs) + "\t" + flt);
      }
    }
    println("wrote " + out);
  }
}
