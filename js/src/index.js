// lowstruct — a strict configuration format in Lowent surface syntax.
//
//   import { parse } from "lowstruct";
//   const doc = parse('server do\n  port 8080 .\nend\n');
//   doc.lookup("server port").one();   // 8080n
//
// The format is defined by spec/lowstruct.md. Values keep their literal kind:
//   int       BigInt (-2^63 .. 2^64-1)      float   Number (finite)     bool  Boolean
//   str       Uint8Array (a "..." literal is a byte string; Leaf.text() gives UTF-8 text)
//   u_str     Array of UTF-16 code units    U_str   Array of code points
//   char / u_char / U_char   Number

export const VERSION = "0.1.0";
export const KINDS = Object.freeze(["int", "float", "bool", "str", "u_str", "U_str", "char", "u_char", "U_char"]);

const VALUE_KINDS = new Set(KINDS);
export const MAX_DEPTH = 64; // nested `do` blocks; deeper is E-LOWS-DEPTH (spec 4.3)
// rem note text do end true false are reserved by the lexer: they never become names.
const ESCAPES = { "\\": 0x5c, '"': 0x22, "'": 0x27, a: 0x07, b: 0x08, f: 0x0c, n: 0x0a, r: 0x0d, t: 0x09, v: 0x0b, 0: 0x00 };
const NAME_RE = /[A-Za-z_][A-Za-z0-9_]*/y;
const NAME_FULL = /^[A-Za-z_][A-Za-z0-9_]*$/;
const D = "[0-9](?:_?[0-9])*";
const H = "[0-9A-Fa-f](?:_?[0-9A-Fa-f])*";
// Lowent annex A.6; a float is tried before an integer.
const NUM_RE = new RegExp(
  `[+-]?(?:(?<hexfloat>0[xX]${H}(?:\\.${H})?[pP][+-]?${D})` +
  `|(?<decfloat>${D}(?:\\.${D}(?:[eE][+-]?${D})?|[eE][+-]?${D}))` +
  `|(?<hex>0[xX]${H})|(?<bin>0[bB][01](?:_?[01])*)|(?<dec>${D}))`, "y");
const INT_MIN = -(1n << 63n);
const INT_MAX = (1n << 64n) - 1n;
const STR_KIND = { "": "str", u: "u_str", U: "U_str" };
const CHAR_KIND = { "": "char", u: "u_char", U: "U_char" };
const WORD_KIND = new Map([["true", "bool"], ["false", "bool"], ["do", "do"], ["end", "end"]]);

const isDigit = (c) => c >= "0" && c <= "9";
const isWord = (c) => c === "_" || (c >= "0" && c <= "9") || (c >= "a" && c <= "z") || (c >= "A" && c <= "Z");

/** A document that is not lowstruct. `code` is the stable diagnostic (E-LOWS-...). */
export class LowsError extends Error {
  constructor(line, col, code, msg) {
    super(`${line}:${col} ${code}: ${msg}`);
    this.name = "LowsError";
    this.line = line;
    this.col = col;
    this.code = code;
    this.msg = msg;
  }
}

/** A key that holds values: all of one kind ("empty" when there are none). */
export class Leaf {
  constructor(kind, values) {
    this.kind = kind;
    this.values = values;
  }

  /** The single value; throws RangeError unless there is exactly one. */
  one() {
    if (this.values.length !== 1) throw new RangeError(`expected exactly one value, found ${this.values.length}`);
    return this.values[0];
  }

  /** The single string value as text (str: strict UTF-8, u_str/U_str: decoded). */
  text() {
    const v = this.one();
    if (this.kind === "str") return new TextDecoder("utf-8", { fatal: true, ignoreBOM: true }).decode(v);
    if (this.kind === "U_str") return String.fromCodePoint(...v);
    if (this.kind === "u_str") return String.fromCharCode(...v);
    throw new TypeError(`not a string: ${this.kind}`);
  }
}

/** A key that holds keys. Iteration follows source order. */
export class Branch extends Map {
  /** Look up a path: "a b c", ["a", "b", "c"] or a single name. Returns undefined when absent. */
  lookup(path) {
    const names = typeof path === "string" ? path.split(/[ \t]+/).filter(Boolean) : path;
    let node = this;
    for (const name of names) {
      if (!(node instanceof Branch) || !node.has(name)) return undefined;
      node = node.get(name);
    }
    return node;
  }
}

/** Parse a document from a string or UTF-8 bytes (Uint8Array/Buffer). Throws LowsError. */
export function parse(src) {
  if (typeof src !== "string") src = decode(src);
  return new Parser(lex(src)).document();
}

function decode(bytes) {
  const bad = firstInvalidUtf8(bytes);
  if (bad >= 0) {
    let lineStart = 0;
    let line = 1;
    for (let k = 0; k < bad; k++) if (bytes[k] === 0x0a) { line++; lineStart = k + 1; }
    const col = [...new TextDecoder("utf-8", { ignoreBOM: true }).decode(bytes.subarray(lineStart, bad))].length + 1;
    throw new LowsError(line, col, "E-LOWS-UTF8", "the file is not valid UTF-8");
  }
  return new TextDecoder("utf-8", { ignoreBOM: true }).decode(bytes);
}

// Index of the first byte that starts an invalid UTF-8 sequence (RFC 3629), or -1.
function firstInvalidUtf8(b) {
  let i = 0;
  while (i < b.length) {
    const c = b[i];
    let need, min;
    if (c < 0x80) { i++; continue; }
    if (c >= 0xc2 && c <= 0xdf) { need = 1; min = 0x80; }
    else if (c >= 0xe0 && c <= 0xef) { need = 2; min = 0x800; }
    else if (c >= 0xf0 && c <= 0xf4) { need = 3; min = 0x10000; }
    else return i;
    let cp = c & (0x3f >> need);
    for (let k = 1; k <= need; k++) {
      const d = b[i + k];
      if (d === undefined || (d & 0xc0) !== 0x80) return i;
      cp = (cp << 6) | (d & 0x3f);
    }
    if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return i;
    i += need + 1;
  }
  return -1;
}

function utf8(str) {
  return new TextEncoder().encode(str);
}

// Round-to-nearest-even of M * 2^E to binary64, exactly. Returns null on overflow.
function hexToDouble(mant, exp) {
  if (mant === 0n) return 0;
  const len = mant.toString(2).length;
  let s = Math.max(len - 53, -1074 - exp);
  let q;
  if (s <= 0) {
    q = mant << BigInt(-s);
  } else {
    const sb = BigInt(s);
    q = mant >> sb;
    const rem = mant & ((1n << sb) - 1n);
    const half = 1n << (sb - 1n);
    if (rem > half || (rem === half && (q & 1n) === 1n)) q += 1n;
    if (q === 1n << 53n) { q = 1n << 52n; s += 1; }
  }
  const e = exp + s;
  if (q.toString(2).length + e > 1024) return null;
  return Number(q) * 2 ** e;
}

class Tok {
  constructor(kind, val, line, col) {
    this.kind = kind;
    this.val = val;
    this.line = line;
    this.col = col;
  }
}

function lex(input) {
  if (input.startsWith("﻿")) throw new LowsError(1, 1, "E-LOWS-BOM", "byte order mark is not allowed; the file is UTF-8 without BOM");
  const s = input.replaceAll("\r\n", "\n");
  const cr = s.indexOf("\r");
  if (cr >= 0) {
    const before = [...s.slice(0, cr)];
    const line = before.filter((c) => c === "\n").length + 1;
    const col = before.length - (before.lastIndexOf("\n") + 1) + 1;
    throw new LowsError(line, col, "E-LOWS-CR", "a lone carriage return is not a line end");
  }
  const src = [...s]; // one element per code point, so columns count code points
  const text = src.join("");
  // Map code-point index -> UTF-16 index for regex matching on `text`.
  const u16 = new Array(src.length + 1);
  for (let k = 0, off = 0; k <= src.length; k++) { u16[k] = off; if (k < src.length) off += src[k].length; }
  const cpAt = (off) => { let lo = 0, hi = src.length; while (lo < hi) { const m = (lo + hi) >> 1; if (u16[m] < off) lo = m + 1; else hi = m; } return lo; };
  const matchAt = (re, at) => { re.lastIndex = u16[at]; return re.exec(text); };
  const n = src.length;
  const toks = [];
  let i = 0, line = 1, lstart = 0;
  const pos = (at) => [line, at - lstart + 1];
  const err = (at, code, msg) => { throw new LowsError(...pos(at), code, msg); };
  const findNl = (from) => { for (let k = from; k < n; k++) if (src[k] === "\n") return k; return -1; };

  const elements = (raw, prefix, at) => {
    const bytes = Uint8Array.from(raw);
    if (prefix === "") return bytes;
    if (firstInvalidUtf8(bytes) >= 0) err(at, "E-LOWS-ESCAPE", "a multi-byte character is cut short in this literal");
    const str = new TextDecoder("utf-8", { ignoreBOM: true }).decode(bytes);
    if (prefix === "U") return [...str].map((c) => c.codePointAt(0));
    const out = [];
    for (let k = 0; k < str.length; k++) out.push(str.charCodeAt(k));
    return out;
  };

  const quoted = (at, prefix) => {
    const q = src[at], start = at - prefix.length;
    let j = at + 1;
    const raw = [];
    for (;;) {
      if (j >= n || src[j] === "\n") err(start, "E-LOWS-UNCLOSED", "literal is not closed on its line");
      const c = src[j];
      if (c === q) { j++; break; }
      if (c === "\\") {
        const e = j + 1 < n ? src[j + 1] : "";
        if (Object.hasOwn(ESCAPES, e)) { raw.push(ESCAPES[e]); j += 2; continue; }
        const width = { x: 2, u: 4, U: 8 }[e];
        const hexpart = width ? src.slice(j + 2, j + 2 + width).join("") : "";
        if (!(width && hexpart.length === width && /^[0-9A-Fa-f]+$/.test(hexpart))) err(j, "E-LOWS-ESCAPE", `escape '\\${e}' is not in the closed set`);
        const v = parseInt(hexpart, 16);
        if (e === "x") raw.push(v);
        else {
          if ((v >= 0xd800 && v <= 0xdfff) || v > 0x10ffff) err(j, "E-LOWS-ESCAPE", "not a code point");
          raw.push(...utf8(String.fromCodePoint(v)));
        }
        j += 2 + width;
        continue;
      }
      raw.push(...utf8(c));
      j++;
    }
    const els = elements(raw, prefix, start);
    if (q === '"') { toks.push(new Tok(STR_KIND[prefix], els, ...pos(start))); return j; }
    if (els.length === 0) err(start, "E-LOWS-CHAR-EMPTY", "an empty character literal");
    if (els.length > 1) err(start, "E-LOWS-CHAR-WIDTH", `this character does not fit one ${{ "": "byte", u: "UTF-16 unit", U: "code point" }[prefix]}`);
    toks.push(new Tok(CHAR_KIND[prefix], els[0], ...pos(start)));
    return j;
  };

  while (i < n) {
    const ch = src[i];
    if (ch === "\n") { i++; line++; lstart = i; continue; }
    if (ch === " " || ch === "\t") { i++; continue; }
    if (ch === ".") { toks.push(new Tok("dot", ".", ...pos(i))); i++; continue; }
    if (ch === '"' || ch === "'") { i = quoted(i, ""); continue; }
    const m = matchAt(NAME_RE, i);
    if (m) {
      const word = m[0];
      const end = i + word.length; // names are ASCII: one code point per UTF-16 unit
      if (end < n && (src[end] === '"' || src[end] === "'")) {
        if (word !== "u" && word !== "U") err(i, "E-LOWS-PREFIX", `\`${word}\` is not a prefix; the prefixes are \`u\` and \`U\``);
        i = quoted(end, word);
        continue;
      }
      if (word === "rem") { const j = findNl(i); i = j < 0 ? n : j; continue; }
      if (word === "note" || word === "text") {
        const start = i;
        const restEnd = findNl(end);
        if (restEnd < 0) err(start, "E-LOWS-UNCLOSED", `\`${word}\` needs a tag and a body`);
        const head = src.slice(end, restEnd).join("").split(/[ \t]+/).filter(Boolean);
        let prefix = "";
        if (word === "text" && head.length === 2 && head.every((h) => NAME_FULL.test(h))) {
          if (head[0] !== "u" && head[0] !== "U") err(start, "E-LOWS-PREFIX", `\`${head[0]}\` is not a prefix; the prefixes are \`u\` and \`U\``);
          prefix = head.shift();
        }
        const tag = head.length === 1 ? head[0] : "";
        if (!NAME_FULL.test(tag)) err(start, "E-LOWS-TAG", `\`${word}\` must be followed by one tag name on the same line`);
        const body = [];
        let j = restEnd + 1, bline = line + 1, k;
        for (;;) {
          if (j >= n) err(start, "E-LOWS-UNCLOSED", `no line holding only \`${tag}\` closes this block`);
          k = findNl(j);
          if (k < 0) k = n;
          const ln = src.slice(j, k).join("");
          if (ln.replace(/^[ \t]+|[ \t]+$/g, "") === tag) break;
          body.push(ln);
          j = k + 1;
          bline++;
        }
        if (word === "text") toks.push(new Tok(STR_KIND[prefix], elements(utf8(body.join("\n")), prefix, start), ...pos(start)));
        i = k;
        line = bline;
        lstart = j;
        continue;
      }
      const kind = WORD_KIND.get(word) ?? "name"; // a Map: `__proto__` is a legal key name
      toks.push(new Tok(kind, kind === "bool" ? word === "true" : word, ...pos(i)));
      i = end;
      continue;
    }
    if (isDigit(ch) || ((ch === "+" || ch === "-") && i + 1 < n && isDigit(src[i + 1]))) {
      const nm = matchAt(NUM_RE, i);
      const end = i + nm[0].length; // ASCII
      if (end < n && isWord(src[end])) err(i, "E-LOWS-NUMBER", "malformed number (a base marker needs a digit, there is no octal, and `_` goes between digits)");
      const clean = nm[0].replaceAll("_", "");
      const g = nm.groups;
      if (g.hexfloat || g.decfloat) {
        let v;
        if (g.hexfloat) {
          const mm = /^([+-]?)0[xX]([0-9A-Fa-f]+)(?:\.([0-9A-Fa-f]+))?[pP]([+-]?[0-9]+)$/.exec(clean);
          const frac = mm[3] ?? "";
          const bin = Number(mm[4]);
          const e = Number.isSafeInteger(bin) ? bin : (bin > 0 ? 1e9 : -1e9);
          const r = hexToDouble(BigInt("0x" + mm[2] + frac), e - 4 * frac.length);
          v = r === null ? Infinity : (mm[1] === "-" ? -r : r);
        } else {
          v = Number(clean);
        }
        if (!Number.isFinite(v)) err(i, "E-LOWS-RANGE", "float is out of range");
        toks.push(new Tok("float", v, ...pos(i)));
      } else {
        const neg = clean.startsWith("-");
        const body = clean.replace(/^[+-]/, "");
        let v = g.dec ? BigInt(body) : BigInt(body.slice(0, 2).toLowerCase() + body.slice(2));
        if (neg) v = -v;
        if (v < INT_MIN || v > INT_MAX) err(i, "E-LOWS-RANGE", "integer is outside -2^63 .. 2^64-1");
        toks.push(new Tok("int", v, ...pos(i)));
      }
      i = end;
      continue;
    }
    err(i, "E-LOWS-CHARSET", `unexpected character ${JSON.stringify(ch)}`);
  }
  return toks;
}

class Parser {
  constructor(toks) {
    this.toks = toks;
    this.i = 0;
    this.sealed = new Map(); // Branch -> id of the block that owns it
    this.open = [];
    this.nextBlock = 0;
  }

  err(t, code, msg) {
    throw new LowsError(t.line, t.col, code, msg);
  }

  document() {
    const root = new Branch();
    this.statements(root, false);
    return root;
  }

  walk(base, path, t, wantLeaf) {
    let node = base;
    for (let k = 0; k < path.length; k++) {
      const name = path[k];
      if (node instanceof Leaf) this.err(t, "E-LOWS-SHAPE", `\`${path.slice(0, k).join(" ")}\` holds values; it cannot also hold keys`);
      const owner = this.sealed.get(node);
      if (owner !== undefined && !this.open.includes(owner)) this.err(t, "E-LOWS-SEALED", `\`${path.slice(0, k).join(" ")}\` was written as a block; add keys inside it`);
      const child = node.get(name);
      const last = k === path.length - 1;
      if (child !== undefined && last) {
        if (!wantLeaf) this.err(t, "E-LOWS-SEALED", `\`${path.join(" ")}\` already exists; a block must be its only writer`);
        if (child instanceof Leaf) this.err(t, "E-LOWS-DUP", `\`${path.join(" ")}\` is already set`);
        this.err(t, "E-LOWS-SHAPE", `\`${path.join(" ")}\` holds keys; it cannot also hold values`);
      }
      if (child === undefined) {
        if (last && wantLeaf) return [node, name];
        const b = new Branch();
        node.set(name, b);
        node = b;
      } else {
        node = child;
      }
    }
    return [node, null];
  }

  statements(base, closing) {
    const toks = this.toks;
    while (this.i < toks.length) {
      const t = toks[this.i];
      if (t.kind === "end") {
        if (!closing) this.err(t, "E-LOWS-END", "`end` without `do`");
        this.i++;
        return;
      }
      if (t.kind !== "name") this.err(t, "E-LOWS-PATH", "a statement starts with a key name");
      const path = [];
      while (this.i < toks.length && toks[this.i].kind === "name") path.push(toks[this.i++].val);
      if (this.i >= toks.length) this.err(t, "E-LOWS-UNCLOSED", "statement is not closed with `.`");
      if (toks[this.i].kind === "do") {
        if (this.open.length >= MAX_DEPTH) this.err(toks[this.i], "E-LOWS-DEPTH", `blocks nest deeper than ${MAX_DEPTH}`);
        const [node] = this.walk(base, path, t, false);
        const id = ++this.nextBlock;
        this.sealed.set(node, id);
        this.open.push(id);
        const doTok = toks[this.i++];
        this.statements(node, true);
        this.open.pop();
        if (node.size === 0) this.err(doTok, "E-LOWS-EMPTY", "empty block");
        continue;
      }
      const vals = [];
      const kinds = new Set();
      while (this.i < toks.length && VALUE_KINDS.has(toks[this.i].kind)) {
        vals.push(toks[this.i].val);
        kinds.add(toks[this.i].kind);
        this.i++;
      }
      if (this.i >= toks.length || toks[this.i].kind !== "dot") {
        const bad = this.i < toks.length ? toks[this.i] : t;
        if (this.i < toks.length && toks[this.i].kind === "name") this.err(bad, "E-LOWS-ORDER", "a key name cannot follow a value; values end the path");
        this.err(bad, "E-LOWS-UNCLOSED", "statement is not closed with `.`");
      }
      if (kinds.size > 1) this.err(t, "E-LOWS-MIX", `one statement holds one kind of value, not ${[...kinds].sort().join(", ")}`);
      this.i++;
      const [parent, name] = this.walk(base, path, t, true);
      parent.set(name, new Leaf(kinds.size ? [...kinds][0] : "empty", vals));
    }
    if (closing) {
      const last = toks[toks.length - 1];
      throw new LowsError(last.line, last.col, "E-LOWS-UNCLOSED", "`do` is not closed with `end`");
    }
  }
}

/** The canonical dump (spec annex C): one line per leaf, identical across implementations. */
export function dumpCanonical(doc) {
  const out = ["lowstruct-dump 1"];
  const hex = (bytes) => Array.from(bytes, (b) => b.toString(16).padStart(2, "0")).join("");
  const value = (kind, v) => {
    if (kind === "int" || kind.endsWith("char")) return String(v);
    if (kind === "float") { const dv = new DataView(new ArrayBuffer(8)); dv.setFloat64(0, v); return hex(new Uint8Array(dv.buffer)); }
    if (kind === "bool") return v ? "true" : "false";
    if (kind === "str") return "x:" + hex(v);
    return "[" + v.join(",") + "]";
  };
  const walk = (node, prefix) => {
    for (const [name, child] of node) {
      const path = [...prefix, name];
      if (child instanceof Leaf) out.push(`${path.join(".")}\t${child.kind}\t${child.values.map((v) => value(child.kind, v)).join(" ")}`);
      else walk(child, path);
    }
  };
  walk(doc, []);
  return out.join("\n") + "\n";
}
