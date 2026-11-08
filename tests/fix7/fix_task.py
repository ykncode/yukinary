with open(r'C:\Users\naitr\.gemini\antigravity-ide\brain\73f266c2-203b-4a0f-9296-53e212b58560\task.md', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('- `[ ]` 5. Compiler Debug String\'leri', '- `[x]` 5. Compiler Debug String\'leri')
text = text.replace('- `[ ]` `main.ykn` ve `codegen.ykn`\'deki gereksiz print\'leri `g_verbose` kontrolü altına al.', '- `[x]` `main.ykn` ve `codegen.ykn`\'deki gereksiz print\'leri `g_verbose` kontrolü altına al.')
text = text.replace('- `[ ]` 6. Stres Testleri', '- `[x]` 6. Stres Testleri')
text = text.replace('- `[ ]` `tests/fix7/stress_recursion.ykn` vb. yaz.', '- `[x]` `tests/fix7/stress_recursion.ykn` vb. yaz.')

with open(r'C:\Users\naitr\.gemini\antigravity-ide\brain\73f266c2-203b-4a0f-9296-53e212b58560\task.md', 'w', encoding='utf-8') as f:
    f.write(text)

print('Task completed.')
