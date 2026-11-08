with open('app_full.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

import re

# Check if they're in the enums
token_enum = re.search(r'enum\s+JetonTipi\s*\{[^}]*\}', content, re.DOTALL)
if token_enum:
    enum_text = token_enum.group()
    print('TIP_SORU_ISLEMI in JetonTipi:', 'TIP_SORU_ISLEMI' in enum_text)

ast_enum = re.search(r'enum\s+AstDugumTipi\s*\{[^}]*\}', content, re.DOTALL)
if ast_enum:
    enum_text = ast_enum.group()
    print('AST_SORU_ISLEMI in AstDugumTipi:', 'AST_SORU_ISLEMI' in enum_text)

# Also check if tokenizer handles '?' character
if 'case' in content and '?' in content:
    # Search for ? handling in tokenizer
    tokenizer = re.search(r"case\s+['\"]\\?['\"]\s*:", content)
    if tokenizer:
        print('Found ? case in tokenizer')
    else:
        print('Looking for ? handling in tokenizer...')
        matches = re.findall(r'.{0,40}\?\.{0,40}', content)
        for m in matches[:5]:
            print('  ', m)