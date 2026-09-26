# Node.js 라이브러리

[English](../manual/javascript.md) · **한국어**

> 정본은 영문 매뉴얼이고, 이 문서는 그 번역입니다.

의존성 없는 ES 모듈이며 Node.js 20 이상에서 돕니다. 패키지는 `js/` 디렉터리입니다.

## 설치

저장소를 받은 뒤:

```sh
npm install ./js
```

## 파일 읽기

```js
import { parse } from "lowstruct";

const doc = parse(Buffer.from(`
server do
  host "0.0.0.0" .
  port 8080 .
  tags "a" "b" .
end
`));

const port = doc.lookup("server port").one();           // 8080n
const host = doc.lookup(["server", "host"]).text();     // "0.0.0.0"
const tags = doc.lookup("server tags").values.map((b) => new TextDecoder().decode(b));
console.log(port, host, tags);
```

`parse()` 는 문자열이나 UTF-8 바이트(`Uint8Array`, `Buffer`)를 받습니다. 되도록 바이트를 넘기십시오. 그래야
잘못된 UTF-8 이 위치와 함께 `E-LOWS-UTF8` 로 보고됩니다.

## 나무

- **가지**는 `Map` 을 이은 `Branch` 입니다. 키는 이름이고 소스 순서를 지킵니다.
  `branch.lookup(path)` 는 `"a b c"` 나 `["a", "b", "c"]` 로 준 경로를 따라가며, 없으면 `undefined` 입니다.
- **잎**은 `kind` 와 `values` 를 가진 `Leaf` 입니다.

| `kind` | JavaScript 값 |
|---|---|
| `int` | `BigInt` — 정수가 2⁶⁴−1 까지라 `Number` 로는 정확히 담지 못합니다 |
| `float` | `Number` |
| `bool` | `Boolean` |
| `str` | `Uint8Array` (바이트 문자열, UTF-8 이 아닐 수 있음) |
| `u_str` · `U_str` | UTF-16 코드 유닛 · 코드포인트의 배열 |
| `char` · `u_char` · `U_char` | `Number` |
| `empty` | 값 없음(`key .`) |

- `Leaf.one()` 은 값 하나를 돌려주고, 값이 정확히 하나가 아니면 `RangeError` 를 던집니다.
- `Leaf.text()` 는 문자열 값 하나를 JavaScript 문자열로 돌려줍니다(`str` 종류는 엄격한 UTF-8).

```js
import { parse } from "lowstruct";

const doc = parse('remote origin do\n  url "https://example.invalid/r.git" .\nend\n');
for (const [name, remote] of doc.get("remote")) console.log(name, remote.lookup("url").text());
```

`__proto__` 나 `constructor` 도 lowstruct 이름이면 키가 될 수 있습니다. 가지가 `Map` 이라 안전합니다.

## 오류

lowstruct 가 아닌 문서는 `LowsError` 를 던집니다. `line`, `col`(코드포인트 단위), `code`(`E-LOWS-…`, 바뀌지 않음),
`msg`(바뀔 수 있음)를 가집니다.

```js
import { parse, LowsError } from "lowstruct";

try {
  parse('port 80 "x" .\n');
} catch (e) {
  if (e instanceof LowsError) console.log(e.line, e.col, e.code); // 1 1 E-LOWS-MIX
}
```

## 그 밖에 내보내는 것

- `dumpCanonical(doc)` — 정규 덤프(명세 부록 C).
- `KINDS`, `MAX_DEPTH`(64), `VERSION`.

## 테스트

`js/` 에서 `npm test`.
