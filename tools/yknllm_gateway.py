#!/usr/bin/env python3
"""Strict boundary between an external LLM and the YKN-only model.

This module is orchestration infrastructure, not an LLM implementation. The
external provider and YKNLLM are injected adapters. Neither adapter receives a
filesystem writer or shell capability; generated YKN is accepted only after a
real compiler process validates it.
"""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import tempfile
import uuid
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Callable, Dict, Optional

try:
    from .gguf_model import GGUFModelError, GGUFModelProfile, read_gguf_profile
except ImportError:  # standalone tools/test invocation
    from gguf_model import GGUFModelError, GGUFModelProfile, read_gguf_profile


class GatewayProtocolError(RuntimeError):
    """The model crossed the role or output contract boundary."""


@dataclass(frozen=True)
class TypedIntent:
    user_text: str
    operation: str = "implement"
    workspace: str = ""
    constraints: tuple[str, ...] = ()


@dataclass(frozen=True)
class YKNLLMResponse:
    kind: str
    code: str
    model_version: str
    confidence: float


@dataclass(frozen=True)
class CompilerResult:
    exit_code: int
    artifact: str
    diagnostics: str


@dataclass(frozen=True)
class AuditRecord:
    correlation_id: str
    intent_sha256: str
    ykn_sha256: str
    model_version: str
    selected_gguf: str
    gguf_architecture: str
    gguf_quantization: str
    compiler_exit_code: int
    accepted: bool
    diagnostics: str


YKNLLMAdapter = Callable[[Dict[str, object]], YKNLLMResponse]


class YKNLLMProviderError(GatewayProtocolError):
    """A configured provider process failed or violated the JSON contract."""


class ProcessYKNLLMAdapter:
    """Run a user-selected YKNLLM provider through a strict JSON process API.

    The executable is invoked with ``shell=False`` and receives one JSON
    request on stdin. It must return exactly one JSON object containing
    ``kind``, ``code``, ``model_version`` and ``confidence``. This adapter is
    deliberately model-agnostic: a provider may use any valid GGUF checkpoint
    described in the request, while its output remains YKN-only.
    """

    def __init__(self, argv: list[str], timeout: float = 120.0):
        if not argv or any(not isinstance(item, str) or not item for item in argv):
            raise YKNLLMProviderError("provider argv must be a non-empty string list")
        if timeout <= 0:
            raise YKNLLMProviderError("provider timeout must be positive")
        self.argv = list(argv)
        self.timeout = timeout

    def __call__(self, request: Dict[str, object]) -> YKNLLMResponse:
        try:
            completed = subprocess.run(
                self.argv,
                input=json.dumps(request, ensure_ascii=False, separators=(",", ":")) + "\n",
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                shell=False,
                timeout=self.timeout,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise YKNLLMProviderError("YKNLLM provider timed out") from exc
        except OSError as exc:
            raise YKNLLMProviderError(f"YKNLLM provider could not start: {exc}") from exc
        if completed.returncode != 0:
            detail = completed.stderr[-4000:].strip()
            raise YKNLLMProviderError(
                f"YKNLLM provider exited with {completed.returncode}: {detail}"
            )
        try:
            payload = json.loads(completed.stdout)
        except json.JSONDecodeError as exc:
            raise YKNLLMProviderError("YKNLLM provider returned invalid JSON") from exc
        if not isinstance(payload, dict):
            raise YKNLLMProviderError("YKNLLM provider response must be a JSON object")
        try:
            return YKNLLMResponse(
                kind=str(payload["kind"]),
                code=str(payload["code"]),
                model_version=str(payload["model_version"]),
                confidence=float(payload["confidence"]),
            )
        except (KeyError, TypeError, ValueError) as exc:
            raise YKNLLMProviderError("YKNLLM provider response fields are invalid") from exc


class YKNLLMContract:
    """Reject non-YKN/model-chat output before compiler invocation."""

    _FORBIDDEN = (
        "```", "#include", "import ", "def ", "function ", "<html", "powershell",
        "cmd.exe", "bash -c", "os.system", "subprocess.",
    )

    @classmethod
    def validate(cls, response: YKNLLMResponse) -> None:
        if response.kind not in {"YKN_CODE", "YKN_PATCH"}:
            raise GatewayProtocolError("YKNLLM response kind must be YKN_CODE or YKN_PATCH")
        if not response.model_version.strip():
            raise GatewayProtocolError("YKNLLM model_version is required")
        if not 0.0 <= response.confidence <= 1.0:
            raise GatewayProtocolError("YKNLLM confidence must be in [0, 1]")
        code = response.code
        if not code.strip() or "\x00" in code:
            raise GatewayProtocolError("YKNLLM code is empty or contains NUL")
        if len(code.encode("utf-8")) > 4 * 1024 * 1024:
            raise GatewayProtocolError("YKNLLM output exceeds the sandbox limit")
        lowered = code.lower()
        for marker in cls._FORBIDDEN:
            if marker.lower() in lowered:
                raise GatewayProtocolError(f"non-YKN output marker rejected: {marker}")
        if not re.search(r"\b(isv|degisken|yapi|ice_aktar)\b", code):
            raise GatewayProtocolError("output does not contain a YKN declaration")


class ExternalLLMGateway:
    """User-facing orchestrator with a compiler-enforced acceptance gate."""

    def __init__(self, compiler: str, audit_path: str, model_path: Optional[str] = None):
        self.compiler = str(Path(compiler).resolve())
        if not Path(self.compiler).is_file():
            raise GatewayProtocolError(f"compiler does not exist: {self.compiler}")
        self.audit_path = Path(audit_path)
        self._model_profile: Optional[GGUFModelProfile] = None
        if model_path:
            self.set_model(model_path)

    @property
    def model_profile(self) -> Optional[GGUFModelProfile]:
        return self._model_profile

    def set_model(self, model_path: str) -> GGUFModelProfile:
        """Select and independently validate the user's GGUF model.

        No model name is hardcoded. A path may point to any valid GGUF v3
        checkpoint, including BitNet-derived files under ``models/``. Model
        bytes are not passed to the YKNLLM contract; only bounded metadata is.
        """
        try:
            profile = read_gguf_profile(model_path)
        except GGUFModelError as exc:
            raise GatewayProtocolError(f"selected GGUF rejected: {exc}") from exc
        self._model_profile = profile
        return profile

    def _compile_sandbox(self, code: str) -> CompilerResult:
        with tempfile.TemporaryDirectory(prefix="yknllm-sandbox-") as temp:
            root = Path(temp)
            source = root / "generated.ykn"
            artifact = root / "generated.exe"
            source.write_text(code, encoding="utf-8")
            completed = subprocess.run(
                [self.compiler, str(source), str(artifact)],
                cwd=str(root),
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=30,
                check=False,
            )
            return CompilerResult(
                exit_code=completed.returncode,
                artifact=str(artifact) if artifact.exists() else "",
                diagnostics=completed.stdout[-16000:],
            )

    def handle(self, intent: TypedIntent, yknllm: YKNLLMAdapter) -> AuditRecord:
        if not intent.user_text.strip():
            raise GatewayProtocolError("user intent cannot be empty")
        correlation_id = str(uuid.uuid4())
        intent_blob = json.dumps(asdict(intent), ensure_ascii=False, sort_keys=True)
        selected_model = None
        if self._model_profile is not None:
            selected_model = {
                "profile": self._model_profile.public(),
                "read_only": True,
                "purpose": "YKN_CODE_ONLY_INFERENCE",
            }
        request = {
            "correlation_id": correlation_id,
            "intent": asdict(intent),
            "role": "YKN_CODE_ONLY",
            "model": selected_model,
            "filesystem_write": False,
            "shell": False,
            "network": False,
        }
        response = yknllm(request)
        if not isinstance(response, YKNLLMResponse):
            raise GatewayProtocolError("YKNLLM adapter returned an invalid response object")
        YKNLLMContract.validate(response)
        result = self._compile_sandbox(response.code)
        accepted = result.exit_code == 0 and bool(result.artifact)
        record = AuditRecord(
            correlation_id=correlation_id,
            intent_sha256=hashlib.sha256(intent_blob.encode("utf-8")).hexdigest(),
            ykn_sha256=hashlib.sha256(response.code.encode("utf-8")).hexdigest(),
            model_version=response.model_version,
            selected_gguf=self._model_profile.path if self._model_profile else "",
            gguf_architecture=self._model_profile.architecture if self._model_profile else "",
            gguf_quantization=self._model_profile.quantization if self._model_profile else "",
            compiler_exit_code=result.exit_code,
            accepted=accepted,
            diagnostics=result.diagnostics,
        )
        self.audit_path.parent.mkdir(parents=True, exist_ok=True)
        with self.audit_path.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(asdict(record), ensure_ascii=False, sort_keys=True) + "\n")
        return record
