// Reports how much of .text is covered by functions/instructions and lists
// pointers from data sections into .text that are not function entries.
//@category thod2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.util.*;

public class ScopeCoverage extends GhidraScript {
  @Override
  protected void run() throws Exception {
    String out = getScriptArgs()[0];
    Memory mem = currentProgram.getMemory();
    Listing listing = currentProgram.getListing();
    FunctionManager fm = currentProgram.getFunctionManager();
    MemoryBlock text = mem.getBlock(".text");
    Address start = text.getStart();
    long size = text.getSize();
    long inFunc = 0, insnOutside = 0, data = 0, undef = 0, pad = 0;
    long undefRunsBig = 0; int undefRunCount = 0; long run = 0;
    Address a = start;
    for (long off = 0; off < size; off++) {
      Address cur = start.add(off);
      boolean f = fm.getFunctionContaining(cur) != null;
      CodeUnit cu = listing.getCodeUnitContaining(cur);
      boolean isInsn = cu instanceof Instruction;
      if (f) { inFunc++; }
      else if (isInsn) { insnOutside++; }
      else if (cu instanceof Data && ((Data) cu).isDefined()) { data++; }
      else {
        byte b = mem.getByte(cur);
        if (b == (byte) 0xCC || b == (byte) 0x90 || b == 0) pad++; else undef++;
      }
      boolean u = !f && !isInsn && !(cu instanceof Data && ((Data) cu).isDefined());
      if (u) run++; else { if (run >= 32) { undefRunsBig += run; undefRunCount++; } run = 0; }
    }
    println(String.format(".text size=%d inFunc=%d insnOutsideFunc=%d data=%d undefNonPad=%d pad=%d undefRuns>=32: %d runs, %d bytes",
      size, inFunc, insnOutside, data, undef, pad, undefRunCount, undefRunsBig));

    // pointers into .text from other blocks
    Set<Long> targets = new TreeSet<>();
    int ptrs = 0;
    try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
      for (MemoryBlock blk : mem.getBlocks()) {
        if (blk == text || !blk.isInitialized()) continue;
        for (long off = 0; off + 4 <= blk.getSize(); off += 4) {
          Address p = blk.getStart().add(off);
          int v = mem.getInt(p);
          long uv = v & 0xffffffffL;
          if (uv >= start.getOffset() && uv < start.getOffset() + size) {
            ptrs++;
            Address t = start.getNewAddress(uv);
            Function fn = fm.getFunctionAt(t);
            Function cont = fm.getFunctionContaining(t);
            String kind = fn != null ? "entry" : (cont != null ? "inside" : "nofunc");
            w.println(blk.getName() + "\t" + p + "\t" + t + "\t" + kind);
            if (fn == null && cont == null) targets.add(uv);
          }
        }
      }
    }
    println("data->text pointers=" + ptrs + " distinct non-function targets=" + targets.size());
  }
}
