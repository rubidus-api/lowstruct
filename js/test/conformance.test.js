// Shared conformance suite (../../conformance).
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync, readdirSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { join, dirname } from "node:path";
import { parse, dumpCanonical, LowsError, Leaf } from "../src/index.js";

const CASES = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "conformance");
const lows = (dir) => readdirSync(join(CASES, dir)).filter((f) => f.endsWith(".lows")).sort();

test("accept: canonical dump matches", () => {
  const files = lows("accept");
  assert.ok(files.length > 0);
  for (const f of files) {
    const doc = parse(readFileSync(join(CASES, "accept", f)));
    const want = readFileSync(join(CASES, "accept", f.replace(/\.lows$/, ".dump")), "utf8");
    assert.equal(dumpCanonical(doc), want, f);
  }
});

test("reject: same diagnostic code", () => {
  const files = lows("reject");
  assert.ok(files.length > 0);
  for (const f of files) {
    const data = readFileSync(join(CASES, "reject", f));
    const want = /expect (E-LOWS-[A-Z0-9-]+)/.exec(data.toString("latin1"))[1];
    assert.throws(() => parse(data), (e) => e instanceof LowsError && e.code === want, `${f}: want ${want}`);
  }
});

test("api: lookup, one, text", () => {
  const doc = parse('server do\n  port 8080 .\n  name "é" .\n  u u"é" .\nend\n');
  assert.equal(doc.lookup("server port").one(), 8080n);
  assert.equal(doc.lookup(["server", "name"]).text(), "é");
  assert.equal(doc.lookup("server u").text(), "é");
  assert.equal(doc.lookup("server nope"), undefined);
  assert.deepEqual([...doc.get("server").keys()], ["port", "name", "u"]);
  const d2 = parse("a 1 2 .\nb .\n__proto__ 3 .\n");
  assert.throws(() => d2.lookup("a").one(), RangeError);
  assert.equal(d2.lookup("b").kind, "empty");
  assert.ok(d2.lookup("__proto__") instanceof Leaf);
});

test("api: error position and UTF-8", () => {
  assert.throws(() => parse('a 1 .\nb 1 "x" .\n'), (e) => e.line === 2 && e.col === 1 && e.code === "E-LOWS-MIX");
  assert.throws(() => parse(Buffer.from([0x61, 0x20, 0x22, 0xff, 0x22, 0x20, 0x2e, 0x0a])), (e) => e.code === "E-LOWS-UTF8" && e.line === 1 && e.col === 4);
  assert.throws(() => parse('é 1 .'), (e) => e.code === "E-LOWS-CHARSET" && e.col === 1);
  assert.throws(() => parse('a "😀" ٣ .'), (e) => e.code === "E-LOWS-CHARSET" && e.col === 7);
});
