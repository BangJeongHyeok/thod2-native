// Merges names given in Ghidra back into re/functions.csv and re/globals.csv.
//
// - Functions with a user-defined name: a new row (status named, confidence
//   guess, evidence ghidra) or the name of an existing row is updated. A
//   user-defined signature is copied when the row has none.
// - User-defined labels outside functions: same for globals.csv.
// - Existing rows are never removed; other columns are kept as they are.
// - New rows get empty notes, which tools/re/check_re.py reports until the
//   notes and doc are filled in by hand. locations.csv is not exported.
//
// Optional script argument: path of the re/ folder.
//@category thod2
import java.io.File;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolType;

public class ExportThod2Re extends GhidraScript {

    @Override
    public void run() throws Exception {
        File reDir = Thod2ReCsv.reDir(getScriptArgs(), getSourceFile().getFile(false));
        String sha256 = currentProgram.getExecutableSHA256();
        if (!Thod2ReCsv.SUPPORTED_SHA256.equalsIgnoreCase(sha256)) {
            printerr("Not the supported Hod2.exe build (SHA-256 " + sha256 + "). Nothing exported.");
            return;
        }
        int changes = exportFunctions(new File(reDir, "functions.csv")) +
            exportGlobals(new File(reDir, "globals.csv"));
        println("THOD2_EXPORT_DONE changes=" + changes);
    }

    private static TreeMap<Long, Map<String, String>> byAddress(List<Map<String, String>> rows) {
        TreeMap<Long, Map<String, String>> map = new TreeMap<>();
        for (Map<String, String> row : rows) {
            map.put(Long.decode(row.get("address")), row);
        }
        return map;
    }

    private static Map<String, String> newRow(String[] columns, long offset, String name) {
        Map<String, String> row = new LinkedHashMap<>();
        for (String column : columns) {
            row.put(column, "");
        }
        row.put("address", Thod2ReCsv.formatAddress(offset));
        row.put("name", name);
        row.put("confidence", "guess");
        row.put("evidence", "ghidra");
        return row;
    }

    private int exportFunctions(File file) throws Exception {
        TreeMap<Long, Map<String, String>> rows =
            byAddress(Thod2ReCsv.read(file, Thod2ReCsv.FUNCTION_COLUMNS));
        int changes = 0;
        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            if (function.getSymbol().getSource() != SourceType.USER_DEFINED) {
                continue;
            }
            long offset = function.getEntryPoint().getOffset();
            String name = function.getName();
            Map<String, String> row = rows.get(offset);
            if (row == null) {
                row = newRow(Thod2ReCsv.FUNCTION_COLUMNS, offset, name);
                row.put("status", "named");
                rows.put(offset, row);
                println("new function " + row.get("address") + " " + name);
                changes++;
            } else if (!row.get("name").equals(name)) {
                println("renamed function " + row.get("address") + " " + row.get("name") +
                    " -> " + name);
                row.put("name", name);
                if (row.get("status").equals("unnamed")) {
                    row.put("status", "named");
                }
                changes++;
            }
            if (row.get("signature").isEmpty() &&
                function.getSignatureSource() == SourceType.USER_DEFINED) {
                row.put("signature", function.getSignature().getPrototypeString(true));
                println("signature " + row.get("address") + " " + row.get("signature"));
                changes++;
            }
        }
        Thod2ReCsv.write(file, Thod2ReCsv.FUNCTION_COLUMNS, new ArrayList<>(rows.values()));
        return changes;
    }

    private int exportGlobals(File file) throws Exception {
        TreeMap<Long, Map<String, String>> rows =
            byAddress(Thod2ReCsv.read(file, Thod2ReCsv.GLOBAL_COLUMNS));
        int changes = 0;
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext()) {
            Symbol symbol = symbols.next();
            if (symbol.getSource() != SourceType.USER_DEFINED ||
                symbol.getSymbolType() != SymbolType.LABEL || !symbol.isPrimary() ||
                !symbol.isGlobal() || isCode(symbol)) {
                continue;
            }
            long offset = symbol.getAddress().getOffset();
            String name = symbol.getName();
            Map<String, String> row = rows.get(offset);
            if (row == null) {
                rows.put(offset, newRow(Thod2ReCsv.GLOBAL_COLUMNS, offset, name));
                println("new global " + Thod2ReCsv.formatAddress(offset) + " " + name);
                changes++;
            } else if (!row.get("name").equals(name)) {
                println("renamed global " + row.get("address") + " " + row.get("name") + " -> " +
                    name);
                row.put("name", name);
                changes++;
            }
        }
        Thod2ReCsv.write(file, Thod2ReCsv.GLOBAL_COLUMNS, new ArrayList<>(rows.values()));
        return changes;
    }

    // Labels in code (jump targets and the like) are not globals.
    private boolean isCode(Symbol symbol) {
        MemoryBlock block = currentProgram.getMemory().getBlock(symbol.getAddress());
        return block == null || block.isExecute() ||
            getFunctionContaining(symbol.getAddress()) != null;
    }
}
