"""End-to-end tests for the Yukinary formatter CLI."""
from pathlib import Path
import subprocess
import sys
import tempfile

SOURCE = (
    "isv ana()->tamsayi{\n"
    "degisken ptr:*u8=0;degisken x:tamsayi=1+2;\n"
    "degisken alan:tamsayi=obj.field;degisken ns:tamsayi=modul::uye;\n"
    "degisken idx:tamsayi=ptr[0];degisken deger:tamsayi=*ptr;\n"
    "eger x>0 ise{\n"
    "# comment x + { } must stay untouched\n"
    "x=x+1;\n"
    "ekrana_yaz(\"x + { }\");\n"
    "}\n"
    "yoksa{dondu 0;}\n"
    "}\n"
)


def run_fmt(compiler: Path, source: Path, *flags: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(compiler), "fmt", *flags, str(source)],
        cwd=compiler.parent,
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: 01_idempotent.py <compiler>", file=sys.stderr)
        return 2

    compiler = Path(sys.argv[1]).resolve()
    if not compiler.is_file():
        print(f"compiler not found: {compiler}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="yukinary_fmt_") as tmp:
        source = Path(tmp) / "fixture.ykn"
        source.write_text(SOURCE, encoding="utf-8", newline="\n")

        first = run_fmt(compiler, source)
        assert first.returncode == 0, first.stderr or first.stdout
        formatted = source.read_text(encoding="utf-8")

        assert "isv ana() -> tamsayi {\n" in formatted
        assert "    degisken ptr: *u8 = 0;\n" in formatted
        assert "    degisken x: tamsayi = 1 + 2;\n" in formatted
        assert "    degisken alan: tamsayi = obj.field;\n" in formatted
        assert "    degisken ns: tamsayi = modul::uye;\n" in formatted
        assert "    degisken idx: tamsayi = ptr[0];\n" in formatted
        assert "    degisken deger: tamsayi = *ptr;\n" in formatted
        assert "        x = x + 1;\n" in formatted
        assert '        ekrana_yaz("x + { }");\n' in formatted
        assert "        # comment x + { } must stay untouched\n" in formatted
        assert "\t" not in formatted

        second = run_fmt(compiler, source)
        assert second.returncode == 0, second.stderr or second.stdout
        assert source.read_text(encoding="utf-8") == formatted

        checked = run_fmt(compiler, source, "--check")
        assert checked.returncode == 0, checked.stderr or checked.stdout
        assert source.read_text(encoding="utf-8") == formatted

        source.write_text(SOURCE, encoding="utf-8", newline="\n")
        before_check = source.read_text(encoding="utf-8")
        needs_formatting = run_fmt(compiler, source, "--check")
        assert needs_formatting.returncode != 0
        assert source.read_text(encoding="utf-8") == before_check

    print("FORMATTER-IDEMPOTENT-PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
