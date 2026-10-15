// Creates functions at data->text pointer targets and at the start of
// instruction runs that no function covers (5 passes at most). Changes the
// program: tools/re/scope.ps1 runs it on a temporary copy of the project.
//@category thod2
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;

public class ScopeRecoverFunctions extends GhidraScript {
  @Override
  protected void run() throws Exception {
    Memory mem = currentProgram.getMemory();
    Listing listing = currentProgram.getListing();
    FunctionManager fm = currentProgram.getFunctionManager();
    MemoryBlock text = mem.getBlock(".text");
    long lo = text.getStart().getOffset(), hi = text.getEnd().getOffset();
    int before = fm.getFunctionCount();
    // 1. pointer targets
    int made = 0;
    for (MemoryBlock blk : mem.getBlocks()) {
      if (blk == text || !blk.isInitialized()) continue;
      for (long off = 0; off + 4 <= blk.getSize(); off += 4) {
        long v = mem.getInt(blk.getStart().add(off)) & 0xffffffffL;
        if (v < lo || v > hi) continue;
        Address t = text.getStart().getNewAddress(v);
        if (fm.getFunctionContaining(t) != null) continue;
        if (listing.getInstructionAt(t) == null) {
          if (listing.getDefinedDataContaining(t) != null) continue;
          new DisassembleCommand(t, null, true).applyTo(currentProgram, monitor);
        }
        if (listing.getInstructionAt(t) != null && new CreateFunctionCmd(t).applyTo(currentProgram, monitor)) made++;
      }
    }
    println("pointer-target functions created: " + made);
    // 2. orphan instruction runs
    for (int pass = 0; pass < 5; pass++) {
      int m2 = 0;
      Address cur = text.getStart();
      while (cur != null && cur.getOffset() <= hi) {
        Instruction ins = listing.getInstructionAt(cur);
        if (ins != null && fm.getFunctionContaining(cur) == null) {
          Instruction prev = listing.getInstructionBefore(cur);
          boolean runStart = prev == null || fm.getFunctionContaining(prev.getAddress()) != null
            || !prev.getMaxAddress().next().equals(cur) || !prev.getFlowType().isFallthrough();
          if (runStart && new CreateFunctionCmd(cur).applyTo(currentProgram, monitor)) {
            m2++;
            Function f = fm.getFunctionAt(cur);
            if (f != null) { cur = f.getBody().getMaxAddress().next(); continue; }
          }
        }
        cur = cur.next();
      }
      println("pass " + pass + " orphan-run functions created: " + m2);
      if (m2 == 0) break;
    }
    println("functions before=" + before + " after=" + fm.getFunctionCount());
  }
}
