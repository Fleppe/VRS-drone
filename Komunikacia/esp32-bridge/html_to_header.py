"""Prevedie data/index.html na esp32_bridge/index_html.h, aby ho ESP32 vedelo poslat.

Spustit po kazdej zmene index.html:
    python html_to_header.py
"""
from pathlib import Path

here = Path(__file__).resolve().parent
html = (here / 'data' / 'index.html').read_text(encoding='utf-8')
delim = 'HTML_END'
assert f'){delim}"' not in html, 'HTML obsahuje ukoncovaci retazec raw literalu'

header = (
    '// VYGENEROVANE z data/index.html skriptom html_to_header.py – needitovat rucne\n'
    '#pragma once\n'
    f'const char INDEX_HTML[] = R"{delim}(' + html + f'){delim}";\n'
)
out = here / 'esp32_bridge' / 'index_html.h'
out.write_text(header, encoding='utf-8', newline='\n')
print(f'{out} ({len(html.encode("utf-8"))} B)')
