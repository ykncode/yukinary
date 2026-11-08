# Yukinary Language Support for Visual Studio Code

Official Visual Studio Code extension for the **Yukinary Programming Language** (`.ykn`).

## Features

- **Multi-Lingual Syntax Highlighting**: Native keyword and syntax colorization across Turkish, English, French, Russian, Japanese, Chinese, and Korean.
- **Language Server Protocol (LSP)**: Automatic autocompletion, hover docs, go-to-definition, diagnostics, and workspace symbol navigation powered by `ykn --lsp`.
- **Code Snippets**: Quick templates for functions (`isv`), loops (`dongu`), conditionals (`eger`/`yoksa`), structures (`yapi`), and imports (`ice_aktar`).
- **One-Click Execution**: Integrated "Run YKN File" button in the editor navigation bar.

## Requirements

The extension communicates automatically with the `ykn` compiler installed at:
- `%USERPROFILE%\.yukinary\bin\ykn.exe` (Windows)
- `~/.yukinary/bin/ykn` (Linux / macOS)
- Or anywhere on your system `PATH`.

## Configuration Settings

- `yukinary.lspExecutable`: Custom absolute path to the `ykn` binary.
- `yukinary.lspArgs`: Arguments passed to the language server (default: `["--lsp"]`).
