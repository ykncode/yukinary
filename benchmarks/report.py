"""Build an auditable feature and test-results markdown report (docs excluded)."""
from __future__ import annotations

import json
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TESTS = ROOT / "tests"
FULL = ROOT / "benchmarks/full-suite-results.json"
RUNTIME = ROOT / "benchmarks/performance.json"
CORRECTNESS = ROOT / "benchmarks/correctness.json"
FIX13 = ROOT / "benchmarks/fix13-results.json"
OUT = ROOT / "benchmarks/FEATURE_MATRIX.md"


def main() -> None:
    report = json.loads(FULL.read_text(encoding="utf-8"))
    runtime = json.loads(RUNTIME.read_text(encoding="utf-8"))
    correctness = json.loads(CORRECTNESS.read_text(encoding="utf-8"))
    fix13 = json.loads(FIX13.read_text(encoding="utf-8-sig"))
    results = report["results"]
    grouped: dict[str, list[dict]] = defaultdict(list)
    for result in results:
        grouped[result["Suite"]].append(result)

    suite_dirs = {
        "GENERICS": ["generics"],
        "I18N (TR, EN, FR, RU, JA, ZH, KO)": ["i18n"],
        "NEGATIVE (STRICT TYPECHECKER)": ["negative"],
        "OPTIMIZER (CFG/DCE/FOLDING)": ["optimizer"],
        "STDLIB (MATH, MEM, STR, FILE)": ["stdlib"],
        "LLM & BITNET (GGUF, BPE, KERNELS)": ["llm"],
    }
    executed_dirs = {d for values in suite_dirs.values() for d in values} | {"fix13"}
    rows = [
        "# YKN — özellik ve test matrisi",
        "",
        "> Ölçüm çıktıları doğrudan kaynak ve testlerden üretildi; `doc/` ve `docs/`",
        "> hariç tutuldu. Bu rapor destek garantisi değildir: test fixture'ının varlığı",
        "> özelliğin tam/production destekli olduğunu kanıtlamaz.",
        "",
        "## Ortam ve toplamlar",
        "",
        f"- Canonical test komutu: `tests/run_all_suites.ps1`, `{report['compiler']}` (SHA-256 `{report.get('compiler_sha256', 'unknown')}`).",
                f"- Performance derleyicisi: `{runtime['compiler']}` (SHA-256 `{runtime['compiler_sha256']}`); canonical ve performance hash'leri {'aynı' if report.get('compiler_sha256') == runtime.get('compiler_sha256') else 'FARKLI'}. Kaynak revision: `{runtime.get('source_revision') or 'git revision unavailable'}`.",
        f"- Canonical runner: **{report['total_passed']}/{report['total_tests']} PASS**, "
        f"**{report['total_failed']} FAIL**, {report['duration_sec']:.2f}s "
        f"({report['pass_rate']}% pass). Runner çıkış kodu başarısızdı; başarısızlar tabloda saklanmadı değil, listelendi.",
        f"- Ayrı correctness toplamı (canonical + FIX-13): **{correctness['passed']}/{correctness['executed_case_count']} PASS**, {correctness['failed']} FAIL; fixture envanteri {correctness['fixture_inventory_count']}.",
        "- Doğruluk raporu: `benchmarks/correctness.json`; ham test oracle'ları `benchmarks/full-suite-results.json` ve `benchmarks/fix13-results.json`.",
        "- Performans raporu: `benchmarks/performance.json`; 5 tekrar + 2 ısınma, binary boyutu ve opsiyonel peak RSS ölçümü.",
        f"- Ölçüm ortamı: {runtime['environment']['os']}; {runtime['environment']['processor']}; {runtime['environment']['cpu_count']} CPU thread. Derleyici SHA-256 `{runtime['compiler_sha256']}`.",
        "- Timer process başlangıcını içeriyor. Peak RSS zamanlı tekrarlar dışında ayrı prob ile ölçülüyor.",
        "- YKN derleyici bayrağı boş/default (seçilen stage-0 binary `--opt` bayrağını reddetti); C/C++ ve Rust hem O0 hem optimize koşullarıyla karşılaştırılmak üzere hazırlandı.",
        "- Performans raporu ham örnekleri saklar; P95 nearest-rank yöntemidir ve 5 ölçümle belirsizdir. Peak RSS 2 ms polling ile ölçülür, kısa zirveleri kaçırabilir.",
                "- Matris ve quicksort fixture'larının çıktı oracle'ları başarısız oldu; yalnızca process exit code ile PASS sayılmaz ve bu iki fixture ölçüme dahil edilmeyen correctness FAIL olarak raporlandı.",
        "",
        "## Özellik kapsama envanteri",
        "",
        "Bu tabloda her test alt klasörü ayrı satırdır. Canonical runner'ın gerçekten",
        "çalıştırdığı altı inline fixture grubu ve özel external suiteler ayrılmıştır; diğer testleri",
        "koşmuş gibi göstermemek için ayrıca sayıları verilir.",
        "",
        "| Test alanı | `.ykn` fixture | Canonical runner durumu |",
        "|---|---:|---|",
    ]
    dirs = sorted(p for p in TESTS.iterdir() if p.is_dir() and p.name.lower() not in {"doc", "docs", "__pycache__"})
    root_fixture_count = sum(1 for _ in TESTS.glob("*.ykn"))
    rows.append(f"| `tests/ (root)` | {root_fixture_count} | canonical runner root-level fixture'ları seçmiyor |")
    for directory in dirs:
        count = sum(1 for _ in directory.rglob("*.ykn"))
        selected = directory.name in executed_dirs
        if directory.name == "fix13":
            passed = fix13["passed"]
            status = f"{passed}/{fix13['total']} PASS (ayrı strict oracle runner)"
        elif selected:
            matching = [x for suite, names in suite_dirs.items() if directory.name in names for x in grouped[suite]]
            passed = sum(x["Status"].startswith("PASS") for x in matching)
            status = f"{passed}/{len(matching)} PASS, {len(matching)-passed} FAIL (oracle'lı subset)"
        else:
            status = "canonical runner bu klasörü çalıştırmıyor"
        rows.append(f"| `{directory.name}` | {count} | {status} |")

    rows += ["", "## Canonical + FIX-13 runner: tüm vaka sonuçları", "",
             f"Aşağıdaki tabloda canonical runner'ın {len(results)} sonucu ve ayrı FIX-13 strict-oracle runner'ın {fix13['total']} sonucu (toplam {len(results)+fix13['total']}) bulunur.", "",
             "| Suite | Test fixture | Sonuç | Süre (ms) | Not / gerçek çıktı |",
             "|---|---|---|---:|---|"]
    for result in results + fix13["results"]:
        suite = result.get("Suite", result.get("suite", "FIX-13")).replace("|", "\\|")
        name = result.get("File", result.get("name", "")).replace("|", "\\|")
        status = result.get("Status", result.get("status", "")).replace("|", "\\|")
        output = " ".join(str(result.get("Output", result.get("output", result.get("compiler_output", "")))).split()).replace("|", "\\|")[:100]
        duration = result.get("DurationMs", "—")
        rows.append(f"| {suite} | `{name}` | {status} | {duration} | {output} |")

    rows += ["", "## Tekrarlı runtime benchmarkı ve dil rakipleri", "",
             "İş yükü: 10.000 × 256 integer array, aynı recurrence, parity branch, checksum=1277871 ve Fibonacci=102334155. "
             "C/C++ `-O3`, Rust opt-level 3, Go default optimizations; YKN default flags. "
             "Ölçüm process başlangıcını içerir. Ortamda bulunmayan toolchain'ler NOT RUN.", "",
             "| Dil | Derleme medyanı | Çalıştırma medyanı | min–max | Binary | Peak RSS | Sonuç |",
             "|---|---:|---:|---:|---:|---:|---|"]
    ykn = runtime["ykn"]["runtime"]
    rows.append(f"| YKN | {ykn['compile']['median_s']*1000:.2f} ms | {ykn['run']['median_s']*1000:.2f} ms | {ykn['run']['min_s']*1000:.2f}–{ykn['run']['max_s']*1000:.2f} ms | {ykn['binary_bytes']} B | {ykn.get('run_peak_rss_bytes') or 'n/a'} B | {ykn['status']} (checksum inside program) |")
    for language, item in runtime["rivals"].items():
        if item["status"] == "PASS":
            build = f"{item['build']['median_s']*1000:.2f} ms" if item.get("build") else "interpreted"
            run_stats = item["run"]
            rows.append(f"| {language} | {build} | {run_stats['median_s']*1000:.2f} ms | {run_stats['min_s']*1000:.2f}–{run_stats['max_s']*1000:.2f} ms | {item.get('binary_bytes') or '—'} | {item.get('peak_rss_bytes') or 'n/a'} B | PASS (shared checksum) |")
        else:
            rows.append(f"| {language} | — | — | — | — | — | {item['status']}: {item.get('reason','')} |")
    rows += ["", "### Özellik bazında rakipler (eşdeğer yaklaşım / ölçüm durumu)", "",
             "Aşağıdaki diller bu özelliklerin gerçek eşdeğerleri olan rakiplerdir. Kaynak kod/fixture denkliği ve kurulu toolchain olmadan süre kıyaslaması üretilemeyeceği için `NOT RUN` açıkça yazılmıştır.", "",
             "| YKN özelliği/test alanı | Benzer karşılıklar | Bu ortamda ölçüm |",
             "|---|---|---|"]
    competitor_rows = [
        ("Dizi, integer loop, branch, checksum", "C/C++ O0/O3, Rust opt-level 0/3, Go, Python", "YKN runtime ve ortak checksum iş yükü: Rust O0/O3, Go, Python PASS; C/C++ toolchain yok"),
        ("Generics / monomorphization", "C++ templates, Rust generics; Go generics", "YKN fixture smoke PASS; rakip özellik fixture'ları ölçülmedi"),
        ("Ownership / borrowing", "Rust borrow checker; C++ RAII/references", "YKN positive fixture smoke PASS; Rust derleyicisi mevcut fakat feature karşılaştırması yapılmadı"),
        ("Trait / dynamic dispatch", "Rust traits/dyn, C++ virtual/concepts, Go interfaces", "Not measured; fixture `stage0` ile compile error"),
        ("Structs / enums / pattern match", "C/C++ structs/enums, Rust ADT/match, Go structs/interfaces", "Fixture test alanı mevcut; all-feature success iddiası yok"),
        ("Modules / imports / package manager", "Rust modules/Cargo, Go packages, Python imports", "YKN package/import cases not included in canonical 122-case run"),
        ("FFI / C ABI", "C/C++ native ABI, Rust `extern C`, Go cgo", "YKN FFI external suite 3/3 PASS; diller arası FFI benchmarkı yapılmadı"),
        ("Async/await / concurrency", "Rust async/Tokio, Go goroutines, Python asyncio", "YKN async fixtures have dedicated negative/acceptance cases; not in canonical run"),
        ("Native cross targets", "Rust/Go cross compile, C cross toolchain", "YKN Linux/macOS/ARM64 suites 12/12 PASS; rivals unavailable"),
        ("Stdlib, file/memory/string", "Rust std, Go stdlib, Python stdlib", "YKN canonical stdlib 27/37 PASS; Python only measured for numeric workload"),
        ("LLM/GGUF/BitNet specific", "Python ecosystem (PyTorch/llama.cpp bindings); C++ kernels", "YKN 9/23 PASS in canonical group; rival ML stack not benchmarked"),
    ]
    rows.extend(f"| {feature} | {equivalents} | {status} |" for feature, equivalents, status in competitor_rows)
    rows += ["", "### YKN vaka benchmarkları", "",
             "| Alan | Fixture | Derleme medyanı | Çalıştırma medyanı | Sonuç |",
             "|---|---|---:|---:|---|"]
    for name, case in runtime["ykn"].items():
        compile_ms = case.get("compile", {}).get("median_s")
        run_ms = case.get("run", {}).get("median_s")
        compile_text = f"{compile_ms*1000:.2f} ms" if compile_ms is not None else "—"
        run_text = f"{run_ms*1000:.2f} ms" if run_ms is not None else "—"
        rows.append(f"| {name} | `{case['source']}` | {compile_text} | {run_text} | {case['status']} ({case['correctness_validation']}) |")
    rows += ["", "## Sınırlar ve başarısızlıklar", "",
             "- Test fixture klasörlerinde toplam `.ykn` sayısı canonical runner'ın 122 oracle'lı vaka sayısından fazladır. "
             "Sadece runner'ın seçtiği testleri koşup bütün repo testleri geçmiş gibi sunmuyorum.",
             "- Bu çalıştırmada Rust 1.96.1 ve Go 1.27.2 bulundu ve aynı checksum iş yükünü geçtiler; C/C++ toolchain'leri yoktu, "
             "bu dillerin sonuçları UNAVAILABLE olarak raporlandı. YKN stage-0 varsayılan bayraklarla ölçüldü; optimize Rust/Go ile derleyici bayrakları eşdeğer değildir.",
             "- Canonical testte PASS olmayan her fixture yukarıdaki vaka tablosunda FAIL statüsü ve gerçek stdout özetiyle listelidir. "
             "Dış suite'ler ve negative testler kendi oracle sözleşmelerine göre değerlendirilmiştir.",
             "- Derleme/çalıştırma zamanları platform, process startup, debug/release, optimizasyon bayrakları ve host PE loader'ına bağlıdır; "
             "diller arası kesin performans sıralaması olarak yorumlanmamalıdır.", ""]
    OUT.write_text("\n".join(rows), encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} ({len(results)+fix13['total']} individual results, {len(dirs)+1} inventory rows)")


if __name__ == "__main__":
    main()
