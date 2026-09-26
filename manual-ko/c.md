# C 라이브러리

[English](../manual/c.md) · **한국어**

> 정본은 영문 매뉴얼이고, 이 문서는 그 번역입니다.

`c/vendor/proven/` 에 들여온 proven_c_lib v0.1.1 위의 C23 입니다. API 는 `c/include/lowstruct.h` 입니다.
오류는 값으로 돌려주고, 모든 할당은 넘겨준 `proven_allocator_t` 를 거치며, 파싱한 문서가 자기 메모리를 모두 가집니다.

## 빌드

```sh
cd c
cc -o ../build/nob nob.c    # 한 번만. 그 뒤로는 nob.c 가 바뀌면 nob 이 스스로 다시 짓습니다
../build/nob                # ../build/c/liblowstruct.a 를 짓고 적합성 사례를 돌립니다
../build/nob lib            # 라이브러리만
```

라이브러리를 쓰려면 `c/include`, `c/vendor/proven/include`, `c/vendor/proven/platform` 을 include 경로에 넣고
`build/c/liblowstruct.a` 를 `-lm` 과 함께 링크합니다. 컴파일러는 `-std=c23` 을 받아야 하며, Linux 의 GCC 14 와
Clang 19 에서 시험했습니다. Windows 빌드는 아직 확인하지 않았습니다.

## 파일 읽기

```c
#include <stdio.h>
#include <string.h>

#include "lowstruct.h"
#include "proven/heap.h"

int main(void) {
    const char *src = "server do\n  host \"0.0.0.0\" .\n  port 8080 .\nend\n";
    lows_doc_t *doc;
    lows_error_t e;
    if (lows_parse(proven_heap_allocator(), (const proven_byte_t *)src, strlen(src), &doc, &e) != PROVEN_OK) {
        fprintf(stderr, "%u:%u %s: %s\n", e.line, e.col, e.code, e.message);
        return 1;
    }
    const lows_node_t *root = lows_doc_root(doc);
    proven_i64 port;
    const proven_byte_t *host;
    proven_size_t host_len;
    if (lows_get_i64(lows_lookup(root, "server port"), &port) == PROVEN_OK &&
        lows_get_bytes(lows_lookup(root, "server host"), &host, &host_len) == PROVEN_OK) {
        printf("%.*s:%lld\n", (int)host_len, (const char *)host, (long long)port);
    }
    lows_doc_free(doc);
    return 0;
}
```


## 파싱

```c
proven_err_t lows_parse(proven_allocator_t alloc, const proven_byte_t *src, proven_size_t len,
                        lows_doc_t **out, lows_error_t *err);
void lows_doc_free(lows_doc_t *doc);
```


| 돌려주는 값 | 뜻 |
|---|---|
| `PROVEN_OK` | `*out` 이 문서입니다. `lows_doc_free` 로 풉니다 |
| `PROVEN_ERR_INVALID_FORMAT` | lowstruct 가 아닙니다. `*err` 에 `line`, `col`(코드포인트), `code`, `message` 가 있습니다 |
| `PROVEN_ERR_NOMEM` | 할당자가 실패했습니다. 새는 메모리는 없습니다 |
| `PROVEN_ERR_INVALID_ARG` | `out` 이 `NULL` 이거나, 길이가 있는데 `src` 가 `NULL` 이거나, 할당자가 온전하지 않습니다 |

`err->code` 와 `err->message` 는 정적 문자열을 가리킵니다. `err` 는 `NULL` 이어도 됩니다. 입력 끝에 NUL 이
없어도 되고, `len` 이 0 이면 `src` 가 `NULL` 이어도 됩니다.

## 나무

| 함수 | 돌려주는 것 |
|---|---|
| `lows_doc_root(doc)` | 뿌리 가지 |
| `lows_lookup(node, "a b c")` | 그 경로(이름을 공백이나 탭으로 가름)의 마디, 없으면 `NULL` |
| `lows_node_is_leaf(node)` · `lows_node_name(node)` | 잎인지 가지인지, 그 이름(뿌리는 `""`) |
| `lows_branch_count(node)` · `lows_branch_child(node, i)` | 자식, 소스 순서 |
| `lows_leaf_kind(node)` · `lows_leaf_count(node)` · `lows_leaf_value(node, i)` | 잎의 종류와 값 |
| `lows_kind_name(kind)` | `"int"`, `"u_str"`, `"empty"`, … |

값은 `lows_value_t` 이며, 살아 있는 멤버는 잎의 종류가 정합니다.

| 종류 | 멤버 |
|---|---|
| `LOWS_KIND_INT` | `i.magnitude`, `i.negative` (음수면 값은 −magnitude) |
| `LOWS_KIND_FLOAT` | `f` |
| `LOWS_KIND_BOOL` | `b` |
| `LOWS_KIND_STR` · `STR16` · `STR32` | `s.ptr`, `s.len` — 바이트, `proven_u16` 유닛, `proven_u32` 코드포인트. `len` 은 원소 수 |
| `LOWS_KIND_CHAR` · `CHAR16` · `CHAR32` | `ch` |
| `LOWS_KIND_EMPTY` | 값이 없는 잎 |

## 값 하나를 읽는 함수

`lows_get_i64`, `lows_get_u64`, `lows_get_f64`, `lows_get_bool`, `lows_get_bytes` 는 값이 정확히 하나인 잎을 읽습니다.

| 돌려주는 값 | 뜻 |
|---|---|
| `PROVEN_OK` | 값을 담았습니다 |
| `PROVEN_ERR_NOT_FOUND` | `NULL`, 가지, 또는 값이 정확히 하나가 아닌 잎 |
| `PROVEN_ERR_INVALID_ARG` | 다른 종류의 잎 |
| `PROVEN_ERR_OVERFLOW` | 정수가 들어가지 않습니다(`i64` 에 `2⁶⁴−1`, `u64` 에 음수) |

`str` 값은 길이를 가진 바이트 문자열입니다. NUL 을 담을 수 있고 UTF-8 이라는 보장도 없으니, C 문자열로
다루지 마십시오.

## 메모리

할당자 위에 풀이 둘 있습니다. 문서의 풀(이름, 마디, 값 — `lows_doc_free` 가 풂)과 토큰용 임시 풀
(`lows_parse` 가 돌아오기 전에 풂)입니다. 블록 중첩이 64 로 제한되므로 재귀 깊이도 제한됩니다. 시험은
AddressSanitizer, LeakSanitizer, UBSan 아래에서 깨끗하게 통과합니다.

## 그 밖의 함수

`lows_dump_canonical(doc, alloc, &text, &len)` 은 정규 덤프(명세 부록 C)를 씁니다. 텍스트는
`alloc.free_fn(alloc.ctx, text)` 로 풉니다. `LOWSTRUCT_VERSION_STRING` 과 `LOWS_MAX_DEPTH` 는 헤더에 있습니다.
