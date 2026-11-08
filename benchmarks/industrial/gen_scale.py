# 25 fonksiyon, her birinde 20 islem = 500 satir kod
def gen_c():
    lines = []
    lines.append("int f0(int x) { return x + 1; }")
    for i in range(1, 25):
        lines.append(f"int f{i}(int x) {{")
        lines.append(f"    int acc = f{i-1}(x);")
        for j in range(20):
            lines.append(f"    acc = acc + {j * 3 + 1};")
        lines.append("    return acc;")
        lines.append("}")
    lines.append("int main() { return f24(10); }")
    return "\n".join(lines)

def gen_rs():
    lines = ["#![allow(dead_code)]", "fn f0(x: i32) -> i32 { x + 1 }"]
    for i in range(1, 25):
        lines.append(f"fn f{i}(x: i32) -> i32 {{")
        lines.append(f"    let mut acc = f{i-1}(x);")
        for j in range(20):
            lines.append(f"    acc = acc + {j * 3 + 1};")
        lines.append("    acc")
        lines.append("}")
    lines.append("fn main() { println!(\"{}\", f24(10)); }")
    return "\n".join(lines)

def gen_ykn():
    lines = []
    lines.append("isv f0(x: tamsayi) -> tamsayi { dondu x + 1; }")
    for i in range(1, 25):
        lines.append(f"isv f{i}(x: tamsayi) -> tamsayi {{")
        lines.append(f"    degisken acc: tamsayi = f{i-1}(x);")
        for j in range(20):
            lines.append(f"    acc = acc + {j * 3 + 1};")
        lines.append("    dondu acc;")
        lines.append("}")
    lines.append("isv ana() -> tamsayi { dondu f24(10); }")
    return "\n".join(lines)

with open('benchmarks/industrial/scale_test.c', 'w') as f: f.write(gen_c())
with open('benchmarks/industrial/scale_test.rs', 'w') as f: f.write(gen_rs())
with open('benchmarks/industrial/scale_test.ykn', 'w') as f: f.write(gen_ykn())

print("Olusturuldu: Her dilde 550 satir, 25 fonksiyon!")
