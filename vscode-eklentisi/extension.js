const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const { LanguageClient, TransportKind } = require('vscode-languageclient/node');

let client = null;

/**
 * Calistirilabilir YKN derleyici binary'sini bul:
 *  1. Kullanici ayarlarinda yukinary.lspExecutable tanimliysa onu kullan
 *  2. ~/.yukinary/bin/ykn(.exe) otomatik kurulum dizininde ara
 *  3. Workspace kokunde ykn.exe / ykn / out.exe / app.exe ara
 *  4. PATH uzerinde 'ykn' ara
 */
function findCompilerExecutable() {
    // 1. Kullanici ayarlari
    const config = vscode.workspace.getConfiguration('yukinary');
    const custom = config.get('lspExecutable', '');
    if (custom && fs.existsSync(custom)) {
        return custom;
    }

    // 2. ~/.yukinary/bin/ otomatik kurulum konumu
    const homeDir = process.env.USERPROFILE || process.env.HOME || '';
    if (homeDir) {
        const isWin = process.platform === 'win32';
        const userYknExe = path.join(homeDir, '.yukinary', 'bin', isWin ? 'ykn.exe' : 'ykn');
        if (fs.existsSync(userYknExe)) {
            return userYknExe;
        }
        const userYknCmd = path.join(homeDir, '.yukinary', 'bin', 'ykn.cmd');
        if (isWin && fs.existsSync(userYknCmd)) {
            return userYknCmd;
        }
    }

    // 3. Workspace kokunde ara
    const folders = vscode.workspace.workspaceFolders;
    if (folders && folders.length > 0) {
        const root = folders[0].uri.fsPath;
        const adaylar = [
            'ykn.exe',
            'ykn',
            'ykn_lsp.exe',
            'out.exe',
            'app.exe',
            'app_full.exe',
            'yukic.exe',
            'yukinary.exe'
        ];
        for (const ad of adaylar) {
            const p = path.join(root, ad);
            if (fs.existsSync(p)) {
                return p;
            }
        }
        const stage0 = path.join(root, 'src', 'stage0', 'app.exe');
        if (fs.existsSync(stage0)) return stage0;
    }

    // 4. PATH uzerinde ykn
    return process.platform === 'win32' ? 'ykn.exe' : 'ykn';
}

/**
 * @param {vscode.ExtensionContext} context
 */
function activate(context) {
    // ================================
    // Run YKN File komutu
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
            const exePath = findCompilerExecutable();
            let runCmd = '';
            if (process.platform === 'win32') {
                runCmd = fs.existsSync(exePath) ? `& "${exePath}" "${filePath}"` : `ykn "${filePath}"`;
            } else {
                runCmd = fs.existsSync(exePath) ? `"${exePath}" "${filePath}"` : `ykn "${filePath}"`;
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
                    PYTHONUNBUFFERED: '1'
                })
            }
        };

        const clientOptions = {
            documentSelector: [
                { scheme: 'file', language: 'yukinary' },
                { scheme: 'untitled', language: 'yukinary' }
            ],
            synchronize: {
                fileEvents: vscode.workspace.createFileSystemWatcher('**/*.ykn')
            },
            outputChannelName: 'Yukinary LSP',
            revealOutputChannelOn: 4 // Hata durumunda goster
        };

        client = new LanguageClient(
            'yukinary',
            'Yukinary Language Server',
            serverOptions,
            clientOptions
        );

        client.start().then(() => {
            // Sessiz veya basarili baslatma
        }).catch(err => {
            vscode.window.showWarningMessage(
                `Yukinary LSP başlatılamadı: ${err}. Syntax highlighting çalışmaya devam edecek.`
            );
        });
    } catch (e) {
        vscode.window.showWarningMessage(
            `Yukinary LSP kurulum uyarısı: ${e.message}. Syntax highlighting aktif.`
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
};
