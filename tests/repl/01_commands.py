"""End-to-end acceptance test for the native Yukinary REPL bridge."""
from pathlib import Path
import subprocess
import sys
import tempfile


LOAD_SOURCE = """isv yukle_deger() -> tamsayi {
    dondu 7;
}
"""


def run_repl(launcher: Path, input_text: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(launcher), "repl"],
        cwd=launcher.parent,
        input=input_text,
        text=True,
        encoding="utf-8",
        capture_output=True,
        timeout=180,
        check=False,
    )


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: 01_commands.py <repl-launcher>", file=sys.stderr)
        return 2

    launcher = Path(sys.argv[1]).resolve()
    if not launcher.is_file():
        print(f"REPL launcher not found: {launcher}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="yukinary_repl_") as tmp:
        load_file = Path(tmp) / "loaded fixture.ykn"
        load_file.write_text(LOAD_SOURCE, encoding="utf-8", newline="\n")
        quoted_load = str(load_file).replace('\\', '/')

        session = "\n".join(
            [
                "degisken x: tamsayi = 41;",
                ".tip x",
                'eger x == 41 ise { ekrana_yaz("STATE\\n".ptr); }',
                "isv iki(deger: tamsayi) -> tamsayi { dondu deger + deger; }",
                ".tip iki(21)",
                'eger iki(21) == 42 ise { ekrana_yaz("FUNC\\n".ptr); }',
                f'.load "{quoted_load}"',
                ".tip yukle_deger()",
                'eger yukle_deger() == 7 ise { ekrana_yaz("LOAD\\n".ptr); }',
                "degisken broken: tamsayi = ;",
                'eger x == 41 ise { ekrana_yaz("AFTER_ERROR\\n".ptr); }',
                ".quit",
                "",
            ]
        )
        result = run_repl(launcher, session)

    output = result.stdout + result.stderr
    if result.returncode != 0:
        print(output[-12000:], file=sys.stderr)
        return 1

    required = [
        "Yukinary REPL",
        "ykn>",
        "tamsayi",
        "STATE",
        "FUNC",
        "Yuklendi",
        "LOAD",
        "Derleme basarisiz",
        "AFTER_ERROR",
    ]
    missing = [item for item in required if item not in output]
    if missing:
        print(f"missing REPL output: {missing}", file=sys.stderr)
        print(output[-12000:], file=sys.stderr)
        return 1

    print("REPL-COMMANDS-PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
