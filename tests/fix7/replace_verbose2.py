import re

def fix_pe():
    with open('src/codegen/output_pe.ykn', 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    for i, line in enumerate(lines):
        if 'ekrana_yaz("PE:' in line and 'eger' not in line:
            lines[i] = '    eger g_verbose == 1 ise { ' + line.strip() + ' }\n'
            
    with open('src/codegen/output_pe.ykn', 'w', encoding='utf-8') as f:
        f.writelines(lines)

def fix_opt():
    with open('src/optimizer.ykn', 'r', encoding='utf-8') as f:
        lines = f.readlines()
        
    for i, line in enumerate(lines):
        if 'ekrana_yaz("    [Opt-Ozet]' in line and 'eger' not in line:
            lines[i] = '    eger g_verbose == 1 ise { ' + line.strip() + ' }\n'
        elif 'sayiyi_ekrana_yaz(' in line and 'eger' not in line:
            # Need to find the context of sayiyi_ekrana_yaz
            if i > 0 and ('Opt-Ozet' in lines[i-1] or 'Opt-Ozet' in lines[i-2] or 'Opt-Ozet' in lines[i-3] or 'silinen:' in lines[i-1]):
                lines[i] = '    eger g_verbose == 1 ise { ' + line.strip() + ' }\n'
        elif 'ekrana_yaz(" | Cebirsel: ' in line and 'eger' not in line:
            lines[i] = '    eger g_verbose == 1 ise { ' + line.strip() + ' }\n'
        elif 'ekrana_yaz("\\n' in line and 'eger' not in line:
            if i > 0 and 'sayiyi_ekrana_yaz' in lines[i-1]:
                lines[i] = '    eger g_verbose == 1 ise { ' + line.strip() + ' }\n'
        elif 'ekrana_yaz(" AST DCE silinen: ' in line and 'eger' not in line:
            lines[i] = '    eger g_verbose == 1 ise { ' + line.strip() + ' }\n'
        elif 'ekrana_yaz(" | MIR DCE silinen: ' in line and 'eger' not in line:
            lines[i] = '    eger g_verbose == 1 ise { ' + line.strip() + ' }\n'
        elif 'ekrana_yaz(" ("' in line and 'eger' not in line:
            lines[i] = '    eger g_verbose == 1 ise { ' + line.strip() + ' }\n'
        elif 'ekrana_yaz(" -> "' in line and 'eger' not in line:
            lines[i] = '    eger g_verbose == 1 ise { ' + line.strip() + ' }\n'
        elif 'ekrana_yaz(")\\n"' in line and 'eger' not in line:
            lines[i] = '    eger g_verbose == 1 ise { ' + line.strip() + ' }\n'

    with open('src/optimizer.ykn', 'w', encoding='utf-8') as f:
        f.writelines(lines)

fix_pe()
fix_opt()
print('Done!')
