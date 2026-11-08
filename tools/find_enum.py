with open('app_full.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

# Find the AstDugumTipi enum definition
idx = content.find('AstDugumTipi')
if idx != -1:
    print('Found at:', idx)
    print(content[idx:idx+200])
else:
    print('NOT FOUND')

# Find enum
idx = content.find('enum')
while idx != -1:
    if 'AstDugumTipi' in content[idx:idx+50]:
        print('Found enum at:', idx)
        print(content[idx:idx+2000])
        break
    idx = content.find('enum', idx+1)
else:
    print('enum not found')