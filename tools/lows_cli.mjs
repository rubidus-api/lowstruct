// Same output as c/tests/lows_cli.c, for the fuzz harness.
import { readFileSync } from "node:fs";
import { parse, dumpCanonical, LowsError } from "../js/src/index.js";
let out = "";
for (const f of process.argv.slice(2)) {
  try { out += dumpCanonical(parse(readFileSync(f))); }
  catch (e) { if (e instanceof LowsError) out += `ERR ${e.code} ${e.line}:${e.col}\n`; else out += `CRASH ${e.name}: ${e.message}\n`; }
  out += "\x1e\n";
}
process.stdout.write(out);
