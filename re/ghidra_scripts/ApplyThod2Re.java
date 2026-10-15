// Applies re/functions.csv, re/globals.csv and re/locations.csv to the current
// program: creates and names functions, applies signatures, labels and types
// globals, and writes comments. Safe to run repeatedly.
//
// Comments written by this script start with "[thod2]" and live in slots the
// script owns: the function comment (plate) of listed functions and the EOL
// comment of listed globals and locations. Put your own notes in PRE/POST
// comments; the owned slots are overwritten on every run.
//
// Optional script argument: path of the re/ folder.
//@category thod2
import java.io.File;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.app.script.GhidraScript;
import ghidra.app.services.DataTypeManagerService;
import ghidra.app.util.cparser.C.CParserUtils;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeManager;
import ghidra.program.model.data.DataUtilities;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.util.data.DataTypeParser;

public class ApplyThod2Re extends GhidraScript {

    private int warnings = 0;

    @Override
    public void run() throws Exception {
        File reDir = Thod2ReCsv.reDir(getScriptArgs(), getSourceFile().getFile(false));
        String sha256 = currentProgram.getExecutableSHA256();
        if (!Thod2ReCsv.SUPPORTED_SHA256.equalsIgnoreCase(sha256)) {
            printerr("Not the supported Hod2.exe build (SHA-256 " + sha256 + "). Nothing applied.");
            return;
        }
        println("Applying " + reDir);
        applyFunctions(Thod2ReCsv.read(new File(reDir, "functions.csv"),
            Thod2ReCsv.FUNCTION_COLUMNS));
        applyGlobals(Thod2ReCsv.read(new File(reDir, "globals.csv"), Thod2ReCsv.GLOBAL_COLUMNS));
        applyLocations(Thod2ReCsv.read(new File(reDir, "locations.csv"),
            Thod2ReCsv.LOCATION_COLUMNS));
        println("THOD2_APPLY_DONE warnings=" + warnings);
    }

    private void warn(String address, String message) {
        warnings++;
        println("WARNING " + address + ": " + message);
    }

    private Address address(Map<String, String> row) {
        return toAddr(Long.decode(row.get("address")));
    }

    private static String footer(Map<String, String> row) {
        StringBuilder text = new StringBuilder();
        text.append("Evidence: ").append(row.get("evidence"));
        if (!row.get("doc").isEmpty()) {
            text.append("; docs/work/").append(row.get("doc").replace(";", ", docs/work/"));
        }
        return text.toString();
    }

    private void applyFunctions(List<Map<String, String>> rows) throws Exception {
        int created = 0;
        for (Map<String, String> row : rows) {
            String at = row.get("address");
            Address entry = address(row);
            String name = row.get("name");
            if (getInstructionAt(entry) == null && !disassemble(entry)) {
                warn(at, "cannot disassemble the function entry");
                continue;
            }
            Function function = getFunctionAt(entry);
            if (function == null) {
                Function containing = getFunctionContaining(entry);
                if (containing != null) {
                    warn(at, "inside " + containing.getName() + " at " +
                        containing.getEntryPoint() + "; creating a function here anyway");
                }
                function = createFunction(entry, name.isEmpty() ? null : name);
                if (function == null) {
                    warn(at, "createFunction failed");
                    continue;
                }
                created++;
            }
            if (!name.isEmpty() && !function.getName().equals(name)) {
                function.setName(name, SourceType.USER_DEFINED);
            }
            if (!row.get("signature").isEmpty()) {
                FunctionDefinitionDataType signature = CParserUtils.parseSignature(
                    (DataTypeManagerService) null, currentProgram, row.get("signature"), true);
                ApplyFunctionSignatureCmd cmd = new ApplyFunctionSignatureCmd(entry, signature,
                    SourceType.USER_DEFINED);
                if (!cmd.applyTo(currentProgram)) {
                    warn(at, "signature not applied: " + cmd.getStatusMsg());
                }
            }
            function.setComment("[thod2] " + row.get("status") + ", " + row.get("confidence") +
                "\n" + row.get("notes") + "\n" + footer(row));
        }
        println("functions: " + rows.size() + " rows, " + created + " created");
    }

    private void applyGlobals(List<Map<String, String>> rows) throws Exception {
        DataTypeManager dtm = currentProgram.getDataTypeManager();
        DataTypeParser parser =
            new DataTypeParser(dtm, dtm, null, DataTypeParser.AllowedDataTypes.ALL);
        for (Map<String, String> row : rows) {
            String at = row.get("address");
            Address address = address(row);
            String name = row.get("name");
            if (!name.isEmpty()) {
                Symbol primary = getSymbolAt(address);
                if (primary == null || !primary.getName().equals(name)) {
                    createLabel(address, name, true, SourceType.USER_DEFINED);
                }
            }
            if (!row.get("type").isEmpty()) {
                try {
                    DataType type = parser.parse(row.get("type"));
                    Data existing = getDataAt(address);
                    if (existing == null || !existing.getDataType().isEquivalent(type)) {
                        DataUtilities.createData(currentProgram, address, type, -1,
                            DataUtilities.ClearDataMode.CLEAR_SINGLE_DATA);
                    }
                } catch (Exception e) {
                    warn(at, "type " + row.get("type") + " not applied: " + e.getMessage());
                }
            }
            setEOLComment(address, "[thod2] " + row.get("confidence") + ": " + row.get("notes") +
                "\n" + footer(row));
        }
        println("globals: " + rows.size() + " rows");
    }

    private void applyLocations(List<Map<String, String>> rows) throws Exception {
        // Several rows can point at the same instruction (e.g. an instruction and
        // its immediate operand), so collect the texts per instruction first.
        Map<Address, List<String>> comments = new LinkedHashMap<>();
        for (Map<String, String> row : rows) {
            String at = row.get("address");
            Address address = address(row);
            Instruction target;
            String text;
            if (row.get("kind").equals("ret")) {
                target = getInstructionBefore(address);
                if (target == null || !target.getFlowType().isCall() ||
                    !target.getMaxAddress().next().equals(address)) {
                    warn(at, "return address does not follow a call instruction");
                    continue;
                }
                text = "returns to " + at + ": " + row.get("notes");
            } else {
                target = getInstructionContaining(address);
                if (target == null) {
                    warn(at, "no instruction here (not disassembled?)");
                    continue;
                }
                text = (target.getAddress().equals(address) ? "" : at + ": ") + row.get("notes");
            }
            comments.computeIfAbsent(target.getAddress(), k -> new ArrayList<>())
                .add(text + " (" + row.get("confidence") + "; " + footer(row) + ")");
        }
        for (Map.Entry<Address, List<String>> entry : comments.entrySet()) {
            setEOLComment(entry.getKey(), "[thod2] " + String.join("\n", entry.getValue()));
        }
        println("locations: " + rows.size() + " rows on " + comments.size() + " instructions");
    }
}
