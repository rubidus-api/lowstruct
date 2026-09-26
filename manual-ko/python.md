# Python 라이브러리

[English](../manual/python.md) · **한국어**

> 정본은 영문 매뉴얼이고, 이 문서는 그 번역입니다.

순수 Python 이며 의존성이 없고, Python 3.10 이상에서 돕니다. 패키지는 `python/lowstruct/` 디렉터리입니다.

## 설치

[릴리스](https://github.com/rubidus-api/lowstruct/releases)에서: `lowstruct-VERSION-python.zip` 을 풀고 `pip install ./lowstruct-VERSION-python`
(또는 그 폴더를 `PYTHONPATH` 에 넣습니다). PyPI 에는 아직 올리지 않았습니다.

저장소를 받은 뒤:

```sh
pip install ./python
```

또는 `python/` 을 `PYTHONPATH` 에 넣습니다.

## 파일 읽기

```python
import lowstruct

doc = lowstruct.loads(b'''
server do
  host "0.0.0.0" .
  port 8080 .
  tags "a" "b" .
end
''')

port = doc.get("server port").one()          # 8080
host = doc.get(("server", "host")).text()     # "0.0.0.0"
tags = [t.decode() for t in doc.get("server tags").values]   # ["a", "b"]
print(port, host, tags)
```

`loads()` 는 `str` 이나 UTF-8 `bytes` 를 받고, `load(fp)` 는 어느 모드로 연 파일 객체든 읽습니다.
되도록 바이트를 넘기십시오. 그래야 잘못된 UTF-8 이 위치와 함께 `E-LOWS-UTF8` 로 보고됩니다.

## 나무

- **가지**는 `dict` 를 이은 `lowstruct.Branch` 입니다. 키는 이름이고 소스 순서를 지킵니다.
  `branch.get(path, default=None)` 은 `"a b c"`, `("a", "b", "c")`, 또는 이름 하나로 준 경로를 따라갑니다.
- **잎**은 `kind` 와 `values` 를 가진 `lowstruct.Leaf` 입니다.

| `kind` | Python 값 |
|---|---|
| `int` | `int` |
| `float` | `float` |
| `bool` | `bool` |
| `str` | `bytes` (바이트 문자열, UTF-8 이 아닐 수 있음) |
| `u_str` · `U_str` | UTF-16 코드 유닛 · 코드포인트의 `list` |
| `char` · `u_char` · `U_char` | `int` |
| `empty` | 값 없음(`key .`) |

- `Leaf.one()` 은 값 하나를 돌려주고, 값이 정확히 하나가 아니면 `LookupError` 를 냅니다.
- `Leaf.text()` 는 문자열 값 하나를 `str` 로 돌려줍니다. `str` 종류는 엄격한 UTF-8 로, `u_str`·`U_str` 는
  원소를 풀어서 만듭니다.

```python
import lowstruct

doc = lowstruct.loads('remote origin do\n  url "https://example.invalid/r.git" .\nend\n')
for name, remote in doc["remote"].items():
    print(name, remote.get("url").text())
```

## 오류

lowstruct 가 아닌 문서는 `ValueError` 를 이은 `lowstruct.LowsError` 를 냅니다. `line`, `col`(코드포인트 단위),
`code`(`E-LOWS-…`, 바뀌지 않음), `msg`(바뀔 수 있음)를 가집니다.

```python
import lowstruct

try:
    lowstruct.loads('port 80 "x" .\n')
except lowstruct.LowsError as e:
    print(e.line, e.col, e.code)   # 1 1 E-LOWS-MIX
```

## 그 밖의 이름

- `lowstruct.dumps_canonical(doc)` — 정규 덤프(명세 부록 C).
- `lowstruct.KINDS`, `lowstruct.MAX_DEPTH`(64), `lowstruct.__version__`.

## 테스트

저장소 루트에서: `python3 -m unittest discover -s python/tests`
