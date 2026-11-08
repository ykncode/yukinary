import re

with open('src/main.ykn', 'r', encoding='utf-8') as f:
    text = f.read()

replacements = [
    ('ekrana_yaz("[3/8] Makrolar genisletiliyor...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[3/8] Makrolar genisletiliyor...\\n".ptr); }'),
    ('ekrana_yaz("[3/8] Makrolar tamamlandi.\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[3/8] Makrolar tamamlandi.\\n".ptr); }'),
    ('ekrana_yaz("[4/8] Generics monomorphization...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[4/8] Generics monomorphization...\\n".ptr); }'),
    ('ekrana_yaz("[4/8] Generics tamamlandi.\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[4/8] Generics tamamlandi.\\n".ptr); }'),
    ('ekrana_yaz("[5/8] Tip denetimi ve Name Resolution...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[5/8] Tip denetimi ve Name Resolution...\\n".ptr); }'),
    ('ekrana_yaz("[5.5/8] Bellek guvenligi analizi...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[5.5/8] Bellek guvenligi analizi...\\n".ptr); }'),
    ('ekrana_yaz("[5.5/8] Guvenlik analizi tamamlandi.\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[5.5/8] Guvenlik analizi tamamlandi.\\n".ptr); }'),
    ('ekrana_yaz("[6/7] AST Optimizasyonu (Constant Folding + DCE)...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[6/7] AST Optimizasyonu (Constant Folding + DCE)...\\n".ptr); }'),
    ('ekrana_yaz("[7/7] x86_64 Makine Kodu uretiliyor...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[7/7] x86_64 Makine Kodu uretiliyor...\\n".ptr); }'),
    ('ekrana_yaz("[7/7] x86_64 Makine Kodu ve PE32+ baglaniyor...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[7/7] x86_64 Makine Kodu ve PE32+ baglaniyor...\\n".ptr); }'),
    ('ekrana_yaz("[7/7] x86_64 Makine Kodu ve ELF64 baglaniyor...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[7/7] x86_64 Makine Kodu ve ELF64 baglaniyor...\\n".ptr); }'),
    ('ekrana_yaz("[7/7] x86_64 Makine Kodu ve Mach-O 64 (macOS) baglaniyor...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[7/7] x86_64 Makine Kodu ve Mach-O 64 (macOS) baglaniyor...\\n".ptr); }'),
    ('ekrana_yaz("[7/7] ARM64 Makine Kodu ve ELF64 (Linux ARM64) baglaniyor...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[7/7] ARM64 Makine Kodu ve ELF64 (Linux ARM64) baglaniyor...\\n".ptr); }'),
    ('ekrana_yaz("[7/7] ARM64 Makine Kodu ve Mach-O 64 (macOS Apple Silicon) baglaniyor...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[7/7] ARM64 Makine Kodu ve Mach-O 64 (macOS Apple Silicon) baglaniyor...\\n".ptr); }'),
    ('ekrana_yaz("[7/7] x86_64 Makine Kodu uretildi (varsayilan PE32+ kullaniliyor)...\\n".ptr);',
     'eger g_verbose == 1 ise { ekrana_yaz("[7/7] x86_64 Makine Kodu uretildi (varsayilan PE32+ kullaniliyor)...\\n".ptr); }'),
]

for old, new in replacements:
    text = text.replace(old, new)

with open('src/main.ykn', 'w', encoding='utf-8') as f:
    f.write(text)

print('Replaced unneeded prints in main.ykn')
