#!/usr/bin/env python3
"""
tools/self_healing_dataset_generator.py
Yukinary (YKN) Self-Healing Dataset Generator — FAZ 9.1

Generates synthetic error variants with deterministic seeds across 6 categories:
  1. SYNTAX: Missing semicolons, unclosed braces, keyword typos
  2. UNDEFINED_SYMBOL: Identifier typos, casing errors
  3. TYPE_MISMATCH: Incompatible assignments, argument count mismatch
  4. NULL_SAFETY: E032 implicit null-to-nonnull, E033 unguarded nullable access
  5. INDEX_ACCESS: Corrupted array index syntax or bad index types
  6. RETURN_TYPE: Mismatched return value in typed functions

Outputs:
  - build/datasets/self_healing_train.jsonl
  - build/datasets/self_healing_heldout_test.jsonl (120+ held-out tests, 20 per category)
"""

import os
import sys
import json
import random
import re

CATEGORIES = [
    "SYNTAX",
    "UNDEFINED_SYMBOL",
    "TYPE_MISMATCH",
    "NULL_SAFETY",
    "INDEX_ACCESS",
    "RETURN_TYPE"
]

KEYWORDS = ["islev", "eger", "ise", "yoksa", "dondu", "degisken", "dongu", "yapi", "sayac"]

BASE_TEMPLATES = [
    {
        "name": "hello_basic",
        "code": """islev ana() -> tamsayi {
    ekrana_yaz("Merhaba Dunya!\\n");
    dondu 0;
}"""
    },
    {
        "name": "arithmetic_add",
        "code": """islev topla(a: tamsayi, b: tamsayi) -> tamsayi {
    degisken sonuc: tamsayi = a + b;
    dondu sonuc;
}

islev ana() -> tamsayi {
    degisken x: tamsayi = topla(10, 20);
    dondu x;
}"""
    },
    {
        "name": "struct_point",
        "code": """yapi Nokta {
    x: tamsayi;
    y: tamsayi;
}

islev ana() -> tamsayi {
    degisken pt: Nokta;
    pt.x = 42;
    pt.y = 84;
    dondu pt.x + pt.y;
}"""
    },
    {
        "name": "nullable_pointer",
        "code": """yapi Dugum {
    deger: tamsayi;
    sonraki: ?*Dugum;
}

islev deger_oku(d: ?*Dugum) -> tamsayi {
    eger d != bos ise {
        dondu d.deger;
    }
    dondu 0;
}

islev ana() -> tamsayi {
    degisken p: ?*Dugum = bos;
    dondu deger_oku(p);
}"""
    },
    {
        "name": "array_index",
        "code": """islev ana() -> tamsayi {
    degisken arr: [5]tamsayi;
    arr[0] = 10;
    arr[1] = 20;
    degisken v: tamsayi = arr[1];
    dondu v;
}"""
    },
    {
        "name": "loop_counter",
        "code": """islev ana() -> tamsayi {
    degisken toplam: tamsayi = 0;
    degisken i: tamsayi = 0;
    dongu (i < 10) {
        toplam = toplam + i;
        i = i + 1;
    }
    dondu toplam;
}"""
    }
]

def mutate_syntax(code: str, rng: random.Random):
    mode = rng.choice(["missing_semicolon", "keyword_typo", "unclosed_brace"])
    lines = code.splitlines()
    corrupted_lines = list(lines)
    explanation = ""
    error_line = 1

    if mode == "missing_semicolon":
        semi_indices = [i for i, line in enumerate(lines) if ";" in line and not line.strip().startswith("#")]
        if semi_indices:
            idx = rng.choice(semi_indices)
            corrupted_lines[idx] = corrupted_lines[idx].replace(";", "", 1)
            error_line = idx + 1
            explanation = f"Missing semicolon at line {error_line}"
    elif mode == "keyword_typo":
        typo_map = {
            "islev": "islv",
            "eger": "egr",
            "dondu": "dndu",
            "degisken": "degsken",
            "yapi": "yaps",
            "dongu": "dngu"
        }
        for i, line in enumerate(lines):
            for kw, typo in typo_map.items():
                if f"{kw} " in line:
                    corrupted_lines[i] = line.replace(f"{kw} ", f"{typo} ", 1)
                    error_line = i + 1
                    explanation = f"Keyword typo '{kw}' -> '{typo}' at line {error_line}"
                    break
            if explanation:
                break
    elif mode == "unclosed_brace":
        brace_indices = [i for i, line in enumerate(lines) if "}" in line]
        if brace_indices:
            idx = rng.choice(brace_indices)
            corrupted_lines[idx] = corrupted_lines[idx].replace("}", "", 1)
            error_line = idx + 1
            explanation = f"Missing closing brace at line {error_line}"

    if not explanation:
        for i, line in enumerate(lines):
            if ";" in line:
                corrupted_lines[i] = line.replace(";", "", 1)
                error_line = i + 1
                explanation = f"Missing semicolon at line {error_line}"
                break

    return "\n".join(corrupted_lines), error_line, explanation

def mutate_undefined_symbol(code: str, rng: random.Random):
    candidates = ["sonuc", "topla", "Nokta", "pt", "Dugum", "deger", "toplam", "arr"]
    for sym in candidates:
        if sym in code:
            typo = sym + "_err"
            corrupted = re.sub(r'\b' + re.escape(sym) + r'\b', typo, code, count=1)
            if corrupted != code:
                lines = corrupted.splitlines()
                err_line = 1
                for i, l in enumerate(lines):
                    if typo in l:
                        err_line = i + 1
                        break
                return corrupted, err_line, f"Undefined symbol typo: '{typo}' instead of '{sym}'"
    return code, 1, "No symbol found"

def mutate_type_mismatch(code: str, rng: random.Random):
    if "topla(10, 20)" in code:
        corrupted = code.replace("topla(10, 20)", 'topla(10, "metin")')
        return corrupted, 7, "Type mismatch: string literal passed where tamsayi expected"
    if "pt.x = 42;" in code:
        corrupted = code.replace("pt.x = 42;", 'pt.x = "kirik";')
        return corrupted, 8, "Type mismatch: string literal assigned to tamsayi field"
    if "arr[0] = 10;" in code:
        corrupted = code.replace("arr[0] = 10;", 'arr[0] = "hata";')
        return corrupted, 3, "Type mismatch: string literal assigned to tamsayi array"
    corrupted = code.replace("10", '"on"')
    return corrupted, 2, "Type mismatch: string literal in arithmetic expression"

def mutate_null_safety(code: str, rng: random.Random):
    # Strip null check guard to cause E033 (unguarded nullable access)
    tmpl = """yapi Dugum {
    deger: tamsayi;
    sonraki: ?*Dugum;
}

islev deger_oku(d: ?*Dugum) -> tamsayi {
    dondu d.deger;
}

islev ana() -> tamsayi {
    degisken p: ?*Dugum = bos;
    dondu deger_oku(p);
}"""
    return tmpl, 7, "E033: Unguarded field access on nullable pointer ?*Dugum"

def mutate_index_access(code: str, rng: random.Random):
    if "arr[1]" in code:
        corrupted = code.replace("arr[1]", 'arr["bir"]')
        return corrupted, 5, "Index access error: string literal used as array index"
    if "arr[0] = 10;" in code:
        corrupted = code.replace("arr[0] = 10;", 'arr[bos] = 10;')
        return corrupted, 3, "Index access error: null pointer used as array index"
    tmpl = """islev ana() -> tamsayi {
    degisken liste: [3]tamsayi;
    liste["gecersiz"] = 5;
    dondu 0;
}"""
    return tmpl, 3, "Index access error: non-integer array index"

def mutate_return_type(code: str, rng: random.Random):
    if "dondu 0;" in code:
        corrupted = code.replace("dondu 0;", 'dondu "sifir";')
        lines = corrupted.splitlines()
        for i, l in enumerate(lines):
            if 'dondu "sifir"' in l:
                return corrupted, i + 1, "Return type mismatch: function expects tamsayi but returns string"
    if "dondu x;" in code:
        corrupted = code.replace("dondu x;", 'dondu;')
        lines = corrupted.splitlines()
        for i, l in enumerate(lines):
            if 'dondu;' in l:
                return corrupted, i + 1, "Return type mismatch: function declared with return type but returns nothing"
    tmpl = """islev hesapla() -> tamsayi {
    dondu "metin";
}

islev ana() -> tamsayi {
    dondu 0;
}"""
    return tmpl, 2, "Return type mismatch: expected tamsayi, got metin"

MUTATORS = {
    "SYNTAX": mutate_syntax,
    "UNDEFINED_SYMBOL": mutate_undefined_symbol,
    "TYPE_MISMATCH": mutate_type_mismatch,
    "NULL_SAFETY": mutate_null_safety,
    "INDEX_ACCESS": mutate_index_access,
    "RETURN_TYPE": mutate_return_type
}

def generate_dataset(seed: int = 42, train_count: int = 300, test_count: int = 120):
    rng = random.Random(seed)
    
    train_records = []
    for i in range(train_count):
        cat = CATEGORIES[i % len(CATEGORIES)]
        template = rng.choice(BASE_TEMPLATES)
        original_code = template["code"]
        corrupted_code, error_line, explanation = MUTATORS[cat](original_code, rng)
        
        record = {
            "id": f"train_{i:04d}",
            "split": "train",
            "category": cat,
            "template_name": template["name"],
            "seed": seed + i,
            "error_line": error_line,
            "explanation": explanation,
            "corrupted_code": corrupted_code,
            "repair_target": original_code
        }
        train_records.append(record)
        
    test_records = []
    tests_per_cat = test_count // len(CATEGORIES)
    for cat_idx, cat in enumerate(CATEGORIES):
        for j in range(tests_per_cat):
            idx = cat_idx * tests_per_cat + j
            test_rng = random.Random(seed + 100000 + idx)
            template = test_rng.choice(BASE_TEMPLATES)
            original_code = template["code"]
            corrupted_code, error_line, explanation = MUTATORS[cat](original_code, test_rng)
            
            record = {
                "id": f"heldout_{idx:04d}",
                "split": "heldout_test",
                "category": cat,
                "template_name": template["name"],
                "seed": seed + 100000 + idx,
                "error_line": error_line,
                "explanation": explanation,
                "corrupted_code": corrupted_code,
                "repair_target": original_code
            }
            test_records.append(record)
            
    return train_records, test_records

def main():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    out_dir = os.path.join(root_dir, "build", "datasets")
    os.makedirs(out_dir, exist_ok=True)
    
    seed = 42
    if len(sys.argv) > 1:
        seed = int(sys.argv[1])
        
    print(f"Generating Self-Healing Dataset with deterministic seed={seed}...")
    train_set, test_set = generate_dataset(seed=seed, train_count=300, test_count=120)
    
    train_path = os.path.join(out_dir, "self_healing_train.jsonl")
    with open(train_path, "w", encoding="utf-8") as f:
        for r in train_set:
            f.write(json.dumps(r, ensure_ascii=False) + "\n")
            
    test_path = os.path.join(out_dir, "self_healing_heldout_test.jsonl")
    with open(test_path, "w", encoding="utf-8") as f:
        for r in test_set:
            f.write(json.dumps(r, ensure_ascii=False) + "\n")
            
    print(f"Dataset generated successfully:")
    print(f"  - Train examples : {len(train_set)} -> {train_path}")
    print(f"  - Held-out tests : {len(test_set)} ({len(test_set)//len(CATEGORIES)} per category) -> {test_path}")
    print(f"  - Categories     : {', '.join(CATEGORIES)}")

if __name__ == "__main__":
    main()
