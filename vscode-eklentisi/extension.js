const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const { LanguageClient, TransportKind } = require('vscode-languageclient');

let client = null;

/**
 * Calistirilabilir YKN derleyici binary'sini bul:
 *  1. Kullanici ayarlarinda yukinary.lspExecutable tanimliysa onu kullan
 *  2. Workspace kokunde out.exe / app.exe / yukic.exe / yukinary.exe ara
 *  3. PATH uzerinde yukic / yukinary ara
 */
function findCompilerExecutable() {
    // 1. Ayarlar
    const config = vscode.workspace.getConfiguration('yukinary');
    const custom = config.get('lspExecutable', '');
    if (custom && fs.existsSync(custom)) {
        return custom;
    }

    // 2. Workspace kokunde ara
    const folders = vscode.workspace.workspaceFolders;
    if (folders && folders.length > 0) {
        const root = folders[0].uri.fsPath;
        const adaylar = [
            'out.exe',
            'app.exe',
            'app_full.exe',
            'yukic.exe',
            'yukinary.exe',
            'stage1.exe',
            'stage2.exe'
        ];
        for (const ad of adaylar) {
            const p = path.join(root, ad);
            if (fs.existsSync(p)) {
                return p;
            }
        }
        // src/stage0/ icinde ara
        const stage0 = path.join(root, 'src', 'stage0', 'app.exe');
        if (fs.existsSync(stage0)) return stage0;
    }

    // 3. PATH uzerinde (isletim sistemi bulsun)
    return 'yukic';
}

/**
 * @param {vscode.ExtensionContext} context
 */
function activate(context) {
    // ================================
    // Run YKN File komutu (eski, korunuyor)
    // ================================
    let terminal = null;
    let disposable = vscode.commands.registerCommand('yukinary.run', function () {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('Çalıştırılacak YKN dosyası bulunamadı!');
            return;
        }
        const document = editor.document;
        if (document.languageId !== 'yukinary') {
            vscode.window.showErrorMessage('Bu bir YKN dosyası değil!');
            return;
        }
        document.save().then(() => {
            if (!terminal) {
                terminal = vscode.window.createTerminal('Yukinary');
            }
            terminal.show();
            const filePath = document.fileName;
            const workspaceFolders = vscode.workspace.workspaceFolders;
            let runCmd = '';
            if (workspaceFolders && workspaceFolders.length > 0) {
                const rootPath = workspaceFolders[0].uri.fsPath;
                const exePath = findCompilerExecutable();
                if (fs.existsSync(exePath)) {
                    runCmd = `& "${exePath}" "${filePath}"`;
                } else {
                    runCmd = `yukic "${filePath}"`;
                }
            } else {
                runCmd = `yukic "${filePath}"`;
            }
            terminal.sendText(runCmd);
        });
    });
    context.subscriptions.push(disposable);

    // ================================
    // LSP Language Client baslat
    // ================================
    try {
        const exe = findCompilerExecutable();
        const config = vscode.workspace.getConfiguration('yukinary');
        const args = config.get('lspArgs', ['--lsp']);

        const serverOptions = {
            command: exe,
            args: args,
            transport: TransportKind.stdio,
            options: {
                shell: false,
                env: Object.assign({}, process.env, {
                    // stdout karismamasi icin konsol ayarlarini standartlastir
                    PYTHONUNBUFFERED: '1'
                })
            }
        };

        // Client secenekleri
        const clientOptions = {
            documentSelector: [
                { scheme: 'file', language: 'yukinary' },
                { scheme: 'untitled', language: 'yukinary' }
            ],
            synchronize: {
                fileEvents: vscode.workspace.createFileSystemWatcher('**/*.ykn')
            },
            outputChannelName: 'Yukinary LSP',
            revealOutputChannelOn: 4  // Error'da goster
        };

        client = new LanguageClient(
            'yukinary',
            'Yukinary Language Server',
            serverOptions,
            clientOptions
        );

        client.start().then(() => {
            vscode.window.showInformationMessage(`Yukinary LSP başlatıldı: ${exe} ${args.join(' ')}`);
        }).catch(err => {
            vscode.window.showWarningMessage(
                `Yukinary LSP başlatılamadı: ${err}. Syntax highlighting çalışmaya devam edecek. ` +
                `LSP için out.exe veya app.exe üretin: g++ app_full.cpp -O3 -o app.exe, sonra app.exe src/main.ykn out.exe`
            );
        });
    } catch (e) {
        vscode.window.showWarningMessage(
            `LSP kurulum hatası: ${e.message}. Extension syntax highlighting ile devam ediyor.`
        );
    }
}

function deactivate() {
    if (client) {
        return client.stop();
    }
    return undefined;
}

module.exports = {
    activate,
    deactivate
}
