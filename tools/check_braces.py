content = open('src/generics.ykn', encoding='utf-8').read()
lines = content.split('\n')
depth = 0
in_func = False
start_line = 0
for i, line in enumerate(lines, 1):
    if 'isv generics_ifade_tara' in line:
        in_func = True
        start_line = i
        print(f'Fonksiyon basliyor satir {i}')
        depth = 0
    if not in_func:
        continue
    # Yorum satırlarını atla
    stripped = line.strip()
    if stripped.startswith('#'):
        continue
    depth += line.count('{') - line.count('}')
    if depth <= 0 and i > start_line + 1:
        print(f'Satir {i} depth={depth}: {line[:100]}')
        if depth == 0:
            print('FONKSIYON BITTI')
            break

print(f'Final depth: {depth}')
print(f'Toplam satir araligi: {start_line}-{i}')
