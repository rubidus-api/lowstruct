# lowstruct

[English](README.md) · **한국어**

작고 엄격한 설정 파일 형식(`.lows`)과, 하나의 적합성 사례를 함께 통과하는 독립된 파서 셋 — **C**, **Node.js**,
**Python** — 입니다.

> 정본은 영문 README 이고, 이 문서는 그 번역입니다.

```lowstruct
rem 서버 설정
title "my-app" .

server do
  host "0.0.0.0" .
  ports 80 443 .            rem 값이 여럿이면 목록입니다
  tls do
    cert "a.pem" .
  end
end

windir "C:\\Windows" .
```

## 왜

- **짐작할 것이 없습니다.** 문장은 이름, 값, 마침표입니다 — `=` 도 괄호도 쉼표도 없습니다. 값은 적힌 종류
  (`int`, `float`, `bool`, 바이트 문자열 …)를 그대로 지니고, 키는 정확히 한 번만 씁니다.
- **역슬래시의 뜻은 하나입니다.** 이스케이프 집합이 닫혀 있어 `"\q"` 는 뜻밖의 값이 아니라 오류입니다.
  여러 줄 원문은 `text TAG … TAG` heredoc 에 적습니다.
- **오류가 정확합니다.** 거부마다 바뀌지 않는 코드(`E-LOWS-…`), 줄, 글자 단위 열을 알려 줍니다. 세 라이브러리가
  같은 자리에서 같은 코드를 냅니다.
- **일부러 작습니다.** `null`, 날짜, 팔진, `inf`/`nan`, BOM 이 없고 UTF-8 만 받습니다.

## 들어 있는 것

| 경로 | 내용 |
|---|---|
| [`spec/`](spec/lowstruct.ko.md) | 명세 판 0.1 — 영문이 규범이고 한국어는 번역 |
| [`manual/`](manual/README.md) · [`manual-ko/`](manual-ko/README.md) | 매뉴얼: 파일 쓰기, 세 라이브러리, 구현하는 사람을 위한 내용 |
| [`conformance/`](manual-ko/implementers.md) | 정규 덤프가 딸린 수용 사례, 기대 코드가 적힌 거부 사례 |
| [`python/`](manual-ko/python.md) | Python 3.10+ 패키지, 의존성 없음 |
| [`js/`](manual-ko/javascript.md) | Node.js 20+ ES 모듈, 의존성 없음 |
| [`c/`](manual-ko/c.md) | proven_c_lib(들여옴) 위의 C23 라이브러리 |
| `tools/` | `test-all.sh` 와 세 구현 차분 퍼즈 |

## 빨리 시작하기

Python:

```python
import lowstruct
doc = lowstruct.loads(open("app.lows", "rb").read())
port = doc.get("server port").one()
```

Node.js:

```js
import { readFileSync } from "node:fs";
import { parse } from "lowstruct";
const port = parse(readFileSync("app.lows")).lookup("server port").one();   // BigInt
```

C:

```c
lows_doc_t *doc;
lows_error_t e;
if (lows_parse(proven_heap_allocator(), src, len, &doc, &e) == PROVEN_OK) {
    proven_i64 port;
    if (lows_get_i64(lows_lookup(lows_doc_root(doc), "server port"), &port) == PROVEN_OK) { /* … */ }
    lows_doc_free(doc);
}
```

## 시험

```sh
tools/test-all.sh          # 세 구현 모두 적합성 사례로
tools/test-all.sh --fuzz   # 차분 퍼즈까지: 같은 덤프, 또는 같은 오류 코드와 위치
```

Python 3.10+, Node.js 20+, `-std=c23` 을 받는 C 컴파일러가 필요합니다(Linux 의 GCC 14 와 Clang 19 에서 시험함).

## 상태

판 0.1, 실험 단계입니다. 형식과 API 가 아직 바뀔 수 있습니다. 로우엔트 `struct` 를 스키마로 쓰는 기능은 다음 판에
넣을 계획입니다(명세 부록 D).

## 로우엔트와의 관계

lowstruct 는 로우엔트(Lowent) 언어의 표면 문법과 리터럴을 빌리지만, 명세·판·진단이 따로 있는 **별개의 프로젝트**
입니다. 로우엔트의 부품이 아니고, `.lows` 파일은 로우엔트 프로그램이 아니며, 어느 라이브러리도 로우엔트를 필요로
하지 않습니다. 리터럴은 로우엔트 언어 개정 1.3 시점에 고정했고, 이후 로우엔트의 변경은 lowstruct 의 새 판을 거쳐서만
들어옵니다.

## 라이선스

MIT — [`LICENSE`](LICENSE) 를 보십시오. 제3자 고지: [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
