// Decompiles every function and records line count and decompiler warnings.
// The output holds only counts, never decompiled code.
//@category thod2
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class ScopeDecompStats extends GhidraScript {
  @Override
  protected void run() throws Exception {
    String out = getScriptArgs()[0];
    DecompInterface di = new DecompInterface();
    di.openProgram(currentProgram);
    int ok = 0, fail = 0;
    try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
      w.println("entry\tok\tlines\twarnings\tunaff\tbad_jump\tcasts");
      for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
        if (monitor.isCancelled()) break;
        DecompileResults r = di.decompileFunction(f, 60, monitor);
        if (r == null || !r.decompileCompleted()) { fail++; w.println(f.getEntryPoint() + "\t0\t0\t0\t0\t0\t0"); continue; }
        ok++;
        String c = r.getDecompiledFunction().getC();
        int lines = 0, warn = 0, unaff = 0, bad = 0, casts = 0;
        for (String l : c.split("\n")) {
          String t = l.trim();
          if (t.isEmpty()) continue;
          lines++;
          if (t.contains("WARNING")) warn++;
          if (t.contains("unaff_")) unaff++;
          if (t.contains("Could not recover jumptable") || t.contains("Bad instruction") || t.contains("halt_baddata")) bad++;
          if (t.contains("(undefined") || t.contains("*(int *)(") || t.contains("*(float *)(")) casts++;
        }
        w.println(f.getEntryPoint() + "\t1\t" + lines + "\t" + warn + "\t" + unaff + "\t" + bad + "\t" + casts);
      }
    }
    println("decompiled ok=" + ok + " fail=" + fail);
  }
}
