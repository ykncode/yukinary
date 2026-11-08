#!/usr/bin/env python3
"""Gateway contract test using the real compiler sandbox."""

import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.yknllm_gateway import (
    ExternalLLMGateway,
    GatewayProtocolError,
    ProcessYKNLLMAdapter,
    TypedIntent,
    YKNLLMResponse,
)


def main() -> int:
    compiler = ROOT / "build" / "agent-stage0.exe"
    if not compiler.is_file():
        raise RuntimeError(f"compiler not found: {compiler}")
    valid_code = "isv ana() -> tamsayi { dondu 0; }\n"

    with tempfile.TemporaryDirectory(prefix="yknllm-gateway-test-") as temp:
        audit = Path(temp) / "audit.jsonl"
        gateway = ExternalLLMGateway(str(compiler), str(audit))

        def valid_adapter(request):
            assert request["role"] == "YKN_CODE_ONLY"
            assert request["filesystem_write"] is False
            return YKNLLMResponse("YKN_CODE", valid_code, "fixture-contract-v1", 0.99)

        record = gateway.handle(TypedIntent("sıfır döndüren ana fonksiyon yaz"), valid_adapter)
        if not record.accepted or record.compiler_exit_code != 0:
            return 1
        if len(audit.read_text(encoding="utf-8").splitlines()) != 1:
            return 2

        def invalid_adapter(_request):
            return YKNLLMResponse("YKN_CODE", "```python\nprint(1)\n```", "bad", 1.0)

        try:
            gateway.handle(TypedIntent("geçersiz çıktı"), invalid_adapter)
            return 3
        except GatewayProtocolError:
            pass

        audit_record = json.loads(audit.read_text(encoding="utf-8").splitlines()[0])
        if not audit_record["accepted"] or not audit_record["correlation_id"]:
            return 4

        provider = Path(temp) / "provider.py"
        provider.write_text(
            "import json, sys\n"
            "request = json.loads(sys.stdin.readline())\n"
            "if request.get('role') != 'YKN_CODE_ONLY': raise SystemExit(3)\n"
            "print(json.dumps({'kind':'YKN_CODE','code':'isv ana() -> tamsayi { dondu 0; }\\n',"
            "'model_version':'process-provider-v1','confidence':0.91}))\n",
            encoding="utf-8",
        )
        process_gateway = ExternalLLMGateway(str(compiler), str(Path(temp) / "process-audit.jsonl"))
        process_record = process_gateway.handle(
            TypedIntent("process provider ile ana yaz"),
            ProcessYKNLLMAdapter([sys.executable, str(provider)]),
        )
        if not process_record.accepted or process_record.model_version != "process-provider-v1":
            return 5

    print("YKNLLM_GATEWAY_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
