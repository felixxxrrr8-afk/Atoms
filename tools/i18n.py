#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Перевод интерфейса «Атомы»: русские строки исходников → английская таблица src/lang_table.inl.

Программа рисует интерфейс на русском; в английском режиме каждая строка, которая попадает в drawText / textW /
fmt / подсказки, ищется в таблице LANG_TABLE по точному совпадению с исходной строкой. Этот скрипт:

  extract [--json FILE]      все узкие строковые литералы с кириллицей (соседние "a" "b" склеиваются, как у компилятора)
  check                      сверка таблицы с исходниками: нет перевода, пустой перевод, разные спецификаторы printf,
                             кириллица в переводе, лишние строки таблицы (код выхода 1 при ошибках)
  todo --json FILE           строки без перевода (для переводчика): id, русский текст, где встречается, контекст
  merge FILE.json [...]      добавить/заменить переводы: {"<id>": "English", ...} или [{"ru": "...", "en": "..."}]
  pseudo --out FILE          псевдоперевод (транслитерация) всех строк — проверка, что вся кириллица на экране
                             проходит через таблицу (в режиме EN на экране не должно остаться ни одной русской буквы)
  rewrite [--prune]          переписать таблицу в каноническом виде (порядок исходников; --prune — убрать лишние)

Ключ строки — её байты после разбора escape-последовательностей (то, что увидит программа), id — первые 10 hex SHA-1.
"""
import argparse
import hashlib
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
TABLE = os.path.join(SRC, "lang_table.inl")
CYR = re.compile(r"[Ѐ-ӿ]")
# флаг «пробел» не распознаётся (в программе не используется): иначе «% от E» → «% of E» выглядело бы как «% o»
PRINTF = re.compile(r"%(?:%|[-+#0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|h|ll|l|z|j|t|L|I64)?[diouxXfFeEgGaAcspn])")


def source_files():
    files = [os.path.join(ROOT, "main.cpp")]
    for name in sorted(os.listdir(SRC)):
        if name.endswith(".inl") and name != "lang_table.inl":
            files.append(os.path.join(SRC, name))
    return files


# ---------------------------------------------------------------- разбор C++ (только то, что нужно для строк)
IDENT = re.compile(r"[A-Za-z0-9_]")


def tokens(text):
    """Последовательность ('S', prefix, raw, line) для строковых литералов и ('O', ch, line) для прочего.
    Комментарии и пробелы пропускаются (соседние литералы через пробелы/комментарии склеиваются)."""
    i, n, line = 0, len(text), 1
    out = []
    while i < n:
        c = text[i]
        if c == "\n":
            line += 1; i += 1; continue
        if c in " \t\r\f\v":
            i += 1; continue
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            line += text.count("\n", i, j); i = j
            continue
        # строковый литерал с необязательным префиксом (не часть идентификатора)
        m = re.match(r'(u8|u|U|L)?"', text[i:i + 3])
        if m and (i == 0 or not IDENT.match(text[i - 1])):
            prefix = m.group(1) or ""
            j = i + len(m.group(0))
            start_line = line
            buf = []
            while j < n and text[j] != '"':
                if text[j] == "\\":
                    buf.append(text[j:j + 2]); j += 2; continue
                if text[j] == "\n":
                    raise SyntaxError("перевод строки внутри литерала, строка %d" % line)
                buf.append(text[j]); j += 1
            out.append(("S", prefix, "".join(buf), start_line))
            i = j + 1
            continue
        if c == "'" and not (i > 0 and text[i - 1].isdigit()):   # символьный литерал (не разделитель разрядов)
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == "\\" else 1
            out.append(("O", "'", line)); i = j + 1
            continue
        out.append(("O", c, line)); i += 1
    return out


def decode(raw):
    """Содержимое литерала (с escape) → байты, как их получит программа при /utf-8."""
    b = bytearray(); i = 0
    simple = {"n": 10, "t": 9, "r": 13, "0": 0, "\\": 92, '"': 34, "'": 39, "?": 63, "a": 7, "b": 8, "f": 12, "v": 11}
    while i < len(raw):
        ch = raw[i]
        if ch != "\\":
            b += ch.encode("utf-8"); i += 1; continue
        nx = raw[i + 1]
        if nx == "x":
            j = i + 2
            while j < len(raw) and raw[j] in "0123456789abcdefABCDEF":
                j += 1
            b.append(int(raw[i + 2:j], 16) & 255); i = j
        elif nx in "01234567":
            j = i + 1
            while j < len(raw) and j < i + 4 and raw[j] in "01234567":
                j += 1
            b.append(int(raw[i + 1:j], 8) & 255); i = j
        elif nx in simple:
            b.append(simple[nx]); i += 2
        elif nx in "uU":
            k = 4 if nx == "u" else 8
            b += chr(int(raw[i + 2:i + 2 + k], 16)).encode("utf-8"); i += 2 + k
        else:
            raise ValueError("неизвестная escape-последовательность \\%s" % nx)
    return bytes(b)


def c_literal(data):
    """Байты → канонический C-литерал в кавычках (UTF-8 как есть, управляющие символы — escape)."""
    try:
        s = data.decode("utf-8")
    except UnicodeDecodeError:
        s = None
    parts, cur = [], []
    if s is not None:
        for ch in s:
            o = ord(ch)
            if ch == "\\": cur.append("\\\\")
            elif ch == '"': cur.append('\\"')
            elif ch == "\n": cur.append("\\n")
            elif ch == "\t": cur.append("\\t")
            elif o < 32 or o == 127: cur.append("\\x%02X" % o); parts.append("".join(cur)); cur = []
            else: cur.append(ch)
    else:
        for o in data:
            cur.append("\\x%02X" % o); parts.append("".join(cur)); cur = []
    parts.append("".join(cur))
    parts = [p for p in parts if p != ""] or [""]
    return " ".join('"%s"' % p for p in parts)


def literal_groups(path):
    """Склеенные литералы файла: (prefix, bytes, line)."""
    with open(path, encoding="utf-8") as f:
        text = f.read()
    res, cur = [], None
    for t in tokens(text):
        if t[0] == "S":
            if cur is not None and cur[0] == t[1]:
                cur[1].append(t[2])
            else:
                if cur is not None:
                    res.append(cur)
                cur = [t[1], [t[2]], t[3]]
        else:
            if cur is not None:
                res.append(cur); cur = None
    if cur is not None:
        res.append(cur)
    out = []
    for prefix, raws, line in res:
        data = b"".join(decode(r) for r in raws)
        out.append((prefix, data, line))
    return out


def sid(data):
    return hashlib.sha1(data).hexdigest()[:10]


def extract():
    """Все русские строки исходников: {bytes: {"id", "ru", "refs": [...], "ctx"}} в порядке первого появления."""
    found = {}
    for path in source_files():
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        with open(path, encoding="utf-8") as f:
            lines = f.read().split("\n")
        for prefix, data, line in literal_groups(path):
            if prefix != "":
                continue
            text = data.decode("utf-8", errors="replace")
            if not CYR.search(text):
                continue
            e = found.get(data)
            if e is None:
                e = found[data] = {"id": sid(data), "ru": text, "refs": [], "ctx": lines[line - 1].strip()[:220]}
            e["refs"].append("%s:%d" % (rel, line))
    return found


# ---------------------------------------------------------------- таблица
def read_table(path=TABLE):
    """Строки таблицы: список (ru_bytes, en_bytes) в порядке файла."""
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        text = f.read()
    k = text.find("LANG_TABLE")
    if k < 0:
        raise SystemExit("в %s нет LANG_TABLE" % path)
    toks = tokens(text[k:])
    rows, i = [], 0
    # {  S...  ,  S...  }
    while i < len(toks):
        if toks[i][0] == "O" and toks[i][1] == "{":
            j = i + 1; a = []
            while j < len(toks) and toks[j][0] == "S":
                a.append(toks[j][2]); j += 1
            if a and j < len(toks) and toks[j][0] == "O" and toks[j][1] == ",":
                j += 1; b = []
                while j < len(toks) and toks[j][0] == "S":
                    b.append(toks[j][2]); j += 1
                if b and j < len(toks) and toks[j][0] == "O" and toks[j][1] == "}":
                    rows.append((b"".join(decode(r) for r in a), b"".join(decode(r) for r in b)))
                    i = j + 1; continue
        i += 1
    return rows


HEADER = """// ===================================== ПЕРЕВОД ИНТЕРФЕЙСА: русский → English ====================
// Ключ — точная русская строка из исходников (после разбора escape), значение — английский перевод.
// Спецификаторы printf (%d, %.2f, %s …) в переводе — те же и в том же порядке: строка может быть форматом fmt().
// Файл поддерживается скриптом tools/i18n.py (check / todo / merge / rewrite); порядок — как в исходниках.
// Новые строки интерфейса: python tools/i18n.py todo --json todo.json → перевести → python tools/i18n.py merge todo.json
static const char* const LANG_TABLE[][2] = {
"""


def write_table(rows_by_key, found, prune=False, path=TABLE):
    """rows_by_key: {ru_bytes: en_bytes}. Порядок: как в исходниках (секции по файлам), затем строки не из исходников."""
    out = [HEADER]
    done = set()
    last_file = None
    for data, e in found.items():
        if data not in rows_by_key:
            continue
        f = e["refs"][0].split(":")[0]
        if f != last_file:
            out.append("    // ---- %s\n" % f); last_file = f
        out.append("    {%s, %s},\n" % (c_literal(data), c_literal(rows_by_key[data])))
        done.add(data)
    rest = [d for d in rows_by_key if d not in done]
    if rest and not prune:
        out.append("    // ---- строки, которых сейчас нет в исходниках (собираются во время работы или устарели)\n")
        for d in rest:
            out.append("    {%s, %s},\n" % (c_literal(d), c_literal(rows_by_key[d])))
    if len(out) == 1:
        out.append('    {"Атомы", "Atoms"},\n')
    out.append("};\n")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("".join(out))


def specs(s):
    return PRINTF.findall(s)


def check(found, rows, strict_unused=False):
    table = {}
    errors, warnings = [], []
    for ru, en in rows:
        if ru in table:
            errors.append("повтор ключа: %s" % ru.decode("utf-8", "replace"))
        table[ru] = en
    for data, e in found.items():
        ref = e["refs"][0]
        if data not in table:
            errors.append("нет перевода [%s] %s: %s" % (e["id"], ref, e["ru"].replace("\n", "\\n")))
            continue
        en = table[data].decode("utf-8", "replace")
        ru = e["ru"]
        if not en.strip():
            errors.append("пустой перевод [%s] %s: %s" % (e["id"], ref, ru.replace("\n", "\\n")))
            continue
        if specs(ru) != specs(en):
            errors.append("разные спецификаторы printf [%s] %s: %s | %s → %s" % (e["id"], ref, ru.replace("\n", "\\n"), specs(ru), specs(en)))
        if CYR.search(en):
            errors.append("кириллица в переводе [%s] %s: %s" % (e["id"], ref, en))
        if ru.count("\n") != en.count("\n"):
            warnings.append("разное число строк (\\n) [%s] %s: %s" % (e["id"], ref, ru.replace("\n", "\\n")))
        if (ru[:1] == " ") != (en[:1] == " ") or (ru[-1:] == " ") != (en[-1:] == " "):
            warnings.append("разные пробелы по краям [%s] %s: «%s» → «%s»" % (e["id"], ref, ru, en))
        if ru.startswith("#") != en.startswith("#"):
            errors.append("префикс # (заголовок справки) не сохранён [%s] %s" % (e["id"], ref))
    for ru in table:
        if ru not in found:
            (errors if strict_unused else warnings).append("строки нет в исходниках: %s" % ru.decode("utf-8", "replace").replace("\n", "\\n"))
    return errors, warnings


TRANSLIT = dict(zip("абвгдеёжзийклмнопрстуфхцчшщъыьэюя",
                    ["a", "b", "v", "g", "d", "e", "yo", "zh", "z", "i", "y", "k", "l", "m", "n", "o", "p", "r", "s", "t", "u", "f", "kh", "ts",
                     "ch", "sh", "shch", "", "y", "", "e", "yu", "ya"]))


def translit(s):
    out = []
    for ch in s:
        lo = ch.lower()
        if lo in TRANSLIT:
            t = TRANSLIT[lo]
            out.append(t.capitalize() if ch != lo else t)
        else:
            out.append(ch)
    return "".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("extract"); p.add_argument("--json")
    p = sub.add_parser("check"); p.add_argument("--strict-unused", action="store_true"); p.add_argument("--quiet-warnings", action="store_true")
    p = sub.add_parser("todo"); p.add_argument("--json", required=True)
    p = sub.add_parser("merge"); p.add_argument("files", nargs="+"); p.add_argument("--replace", action="store_true", help="заменять уже переведённые")
    p = sub.add_parser("pseudo"); p.add_argument("--out", required=True)
    p = sub.add_parser("rewrite"); p.add_argument("--prune", action="store_true")
    a = ap.parse_args()

    found = extract()
    if a.cmd == "extract":
        items = [dict(e) for e in found.values()]
        if a.json:
            with open(a.json, "w", encoding="utf-8") as f:
                json.dump(items, f, ensure_ascii=False, indent=1)
        print("строк с кириллицей: %d (в %d местах)" % (len(items), sum(len(e["refs"]) for e in items)))
        return 0
    rows = read_table()
    if a.cmd == "check":
        errors, warnings = check(found, rows, a.strict_unused)
        if not a.quiet_warnings:
            for w in warnings:
                print("предупреждение:", w)
        for e in errors:
            print("ОШИБКА:", e)
        print("строк в исходниках: %d, в таблице: %d, ошибок: %d, предупреждений: %d" % (len(found), len(rows), len(errors), len(warnings)))
        return 1 if errors else 0
    if a.cmd == "todo":
        have = {ru for ru, en in rows if en.strip()}
        items = [dict(e) for d, e in found.items() if d not in have]
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump(items, f, ensure_ascii=False, indent=1)
        print("без перевода: %d → %s" % (len(items), a.json))
        return 0
    if a.cmd == "merge":
        by_id = {e["id"]: d for d, e in found.items()}
        table = {ru: en for ru, en in rows}
        added = replaced = 0
        for fn in a.files:
            with open(fn, encoding="utf-8") as f:
                data = json.load(f)
            pairs = []
            if isinstance(data, dict):
                for k, v in data.items():
                    if k in by_id:
                        pairs.append((by_id[k], v))
                    else:
                        print("предупреждение: id %s не найден в исходниках (%s)" % (k, fn))
            else:
                for it in data:
                    pairs.append((it["ru"].encode("utf-8"), it["en"]))
            for ru, en in pairs:
                enb = en.encode("utf-8")
                if ru in table and table[ru].strip():
                    if a.replace and table[ru] != enb:
                        table[ru] = enb; replaced += 1
                    continue
                table[ru] = enb; added += 1
        write_table(table, found)
        print("добавлено: %d, заменено: %d, всего в таблице: %d" % (added, replaced, len(table)))
        return 0
    if a.cmd == "pseudo":
        table = {d: translit(e["ru"]).encode("utf-8") for d, e in found.items()}
        write_table(table, found, path=a.out)
        print("псевдоперевод: %d строк → %s" % (len(table), a.out))
        return 0
    if a.cmd == "rewrite":
        write_table({ru: en for ru, en in rows}, found, prune=a.prune)
        print("таблица переписана: %d строк" % len(rows))
        return 0


if __name__ == "__main__":
    sys.exit(main())
