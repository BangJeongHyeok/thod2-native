// Shared helpers for the thod2-native Ghidra scripts: re/ folder lookup and
// RFC 4180 CSV reading and writing (the format tools/re/check_re.py checks).
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class Thod2ReCsv {

    // SHA-256 of the supported Hod2.exe build (AGENTS.md, section 3).
    public static final String SUPPORTED_SHA256 =
        "1d71265096661cf034568ff71184fcbbed0cd9aea0e5e2f041801c39a0ffce69";

    public static final String[] FUNCTION_COLUMNS =
        { "address", "name", "status", "confidence", "signature", "evidence", "doc", "notes" };
    public static final String[] GLOBAL_COLUMNS =
        { "address", "name", "type", "confidence", "evidence", "doc", "notes" };
    public static final String[] LOCATION_COLUMNS =
        { "address", "kind", "confidence", "evidence", "doc", "notes" };

    private Thod2ReCsv() {
    }

    // The re/ folder: the first script argument, or the parent of the folder
    // that holds this script (re/ghidra_scripts/..).
    public static File reDir(String[] args, File scriptFile) {
        if (args != null && args.length > 0 && !args[0].isEmpty()) {
            return new File(args[0]);
        }
        return scriptFile.getAbsoluteFile().getParentFile().getParentFile();
    }

    public static String formatAddress(long offset) {
        return String.format("0x%06X", offset);
    }

    public static List<Map<String, String>> read(File file, String[] columns) throws IOException {
        String text = new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
        List<List<String>> records = parse(text);
        if (records.isEmpty() || !records.get(0).equals(List.of(columns))) {
            throw new IOException(file + ": header must be " + String.join(",", columns));
        }
        List<Map<String, String>> rows = new ArrayList<>();
        for (int i = 1; i < records.size(); i++) {
            List<String> values = records.get(i);
            if (values.size() == 1 && values.get(0).isEmpty()) {
                continue;
            }
            if (values.size() != columns.length) {
                throw new IOException(file + ": record " + (i + 1) + " has " + values.size() +
                    " fields, expected " + columns.length);
            }
            Map<String, String> row = new LinkedHashMap<>();
            for (int c = 0; c < columns.length; c++) {
                row.put(columns[c], values.get(c));
            }
            rows.add(row);
        }
        return rows;
    }

    public static void write(File file, String[] columns, List<Map<String, String>> rows)
            throws IOException {
        StringBuilder out = new StringBuilder();
        appendRecord(out, List.of(columns));
        for (Map<String, String> row : rows) {
            List<String> values = new ArrayList<>();
            for (String column : columns) {
                values.add(row.getOrDefault(column, ""));
            }
            appendRecord(out, values);
        }
        Files.write(file.toPath(), out.toString().getBytes(StandardCharsets.UTF_8));
    }

    private static void appendRecord(StringBuilder out, List<String> values) {
        for (int i = 0; i < values.size(); i++) {
            if (i > 0) {
                out.append(',');
            }
            String value = values.get(i);
            if (value.contains(",") || value.contains("\"") || value.contains("\n") ||
                value.contains("\r")) {
                out.append('"').append(value.replace("\"", "\"\"")).append('"');
            } else {
                out.append(value);
            }
        }
        out.append('\n');
    }

    private static List<List<String>> parse(String text) {
        List<List<String>> records = new ArrayList<>();
        List<String> record = new ArrayList<>();
        StringBuilder field = new StringBuilder();
        boolean quoted = false;
        for (int i = 0; i < text.length(); i++) {
            char ch = text.charAt(i);
            if (quoted) {
                if (ch == '"') {
                    if (i + 1 < text.length() && text.charAt(i + 1) == '"') {
                        field.append('"');
                        i++;
                    } else {
                        quoted = false;
                    }
                } else {
                    field.append(ch);
                }
            } else if (ch == '"') {
                quoted = true;
            } else if (ch == ',') {
                record.add(field.toString());
                field.setLength(0);
            } else if (ch == '\n' || ch == '\r') {
                if (ch == '\r' && i + 1 < text.length() && text.charAt(i + 1) == '\n') {
                    i++;
                }
                record.add(field.toString());
                field.setLength(0);
                records.add(record);
                record = new ArrayList<>();
            } else {
                field.append(ch);
            }
        }
        if (field.length() > 0 || !record.isEmpty()) {
            record.add(field.toString());
            records.add(record);
        }
        return records;
    }
}
