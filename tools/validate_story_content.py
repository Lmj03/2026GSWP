"""Static asset/registration validation; does not build or run the game."""
from pathlib import Path
import math
import re
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
locale = {}
for line_no, line in enumerate((root / 'SimpleGame/Data/Locale/ko.txt').read_text(encoding='utf-8-sig').splitlines(), 1):
    if not line or line.startswith('#'):
        continue
    key, separator, value = line.partition('=')
    if not separator or key in locale:
        raise ValueError(f'Locale duplicate/malformed key at line {line_no}: {key}')
    locale[key] = value

sources = list((root / 'SimpleGame').glob('*.cpp')) + list((root / 'SimpleGame').glob('*.h'))
for path in sources:
    text = path.read_text(encoding='utf-8-sig')
    for key in re.findall(r'"((?:story|choice|npc|journal|world|monitor|hud|town)\.[A-Za-z0-9_.]+)"', text):
        if not key.endswith('.') and key not in locale:
            raise ValueError(f'{path.name}: missing locale {key}')
for prefix, count in [('npc.job.', 7), ('npc.activity.', 11), ('npc.info.', 7), ('story.objective.', 8), ('world.sign.', 5)]:
    for index in range(count):
        if prefix + str(index) not in locale:
            raise ValueError(f'Missing dynamic key {prefix}{index}')

buildings = []
chunks = set()
for raw in (root / 'SimpleGame/Data/afterglow.district').read_text(encoding='utf-8-sig').splitlines():
    fields = raw.partition('#')[0].split()
    if fields and fields[0] == 'building':
        buildings.append(tuple(map(float, fields[2:6])))
    if fields and fields[0] == 'chunk':
        chunks.add(tuple(map(int, fields[1:3])))

nodes = {}
for raw in (root / 'SimpleGame/Data/Story/act1.story').read_text(encoding='utf-8-sig').splitlines():
    fields = raw.partition('#')[0].split()
    if not fields:
        continue
    if fields[0] == 'version':
        assert fields == ['version', '1']
    else:
        assert len(fields) == 4 and fields[0] == 'node'
        key = fields[1]
        assert key not in nodes
        nodes[key] = tuple(map(float, fields[2:4]))
assert set(nodes) == {'crime', 'terminal', 'family', 'witness', 'target', 'rebels'}
for key, (x, y) in nodes.items():
    assert math.isfinite(x) and math.isfinite(y)
    for dx, dy in [(-.3, -.3), (.3, -.3), (-.3, .3), (.3, .3)]:
        assert (math.floor((x + dx) / 8), math.floor((y + dy) / 8)) in chunks, key
    for bx, by, w, d in buildings:
        assert not (bx - .3 < x < bx + w + .3 and by - .3 < y < by + d + .3), key
    assert 'story.marker.' + key in locale

ns = {'ms': 'http://schemas.microsoft.com/developer/msbuild/2003'}
for project in ['SimpleGame.vcxproj', 'SimpleGame.vcxproj.filters']:
    tree = ET.parse(root / 'SimpleGame' / project)
    for extension, kind in [('.cpp', 'ClCompile'), ('.h', 'ClInclude')]:
        registered = [item.attrib['Include'] for item in tree.findall('.//ms:' + kind, ns) if 'Include' in item.attrib]
        for path in (root / 'SimpleGame').glob('*' + extension):
            assert registered.count(path.name) == 1, (project, path.name)

text = '\n'.join(p.read_text(encoding='utf-8-sig') for p in (root / 'SimpleGame').glob('*.cpp'))
definitions = re.findall(r'CityScene::(\w+)\s*\(', text)
assert len(definitions) == len(set(definitions)), 'Duplicate CityScene definition'
header = (root / 'SimpleGame/CityScene.h').read_text(encoding='utf-8-sig')
for name in re.findall(r'^\s+(?:[\w:<>]+\s+)?(\w+)\([^;{}]*\)(?: const)?;', header, re.M):
    assert name in definitions, 'Missing CityScene definition: ' + name
print(f'Static validation OK: {len(locale)} locale keys, {len(nodes)} story nodes, project entries and scene definitions')
