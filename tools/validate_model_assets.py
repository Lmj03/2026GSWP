"""Inspect delivery files and project registration without building/running the game.

This checks the import contract, not the C++ loader or OpenGL output.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import struct
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
COMPONENT = {5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2), 5123: ('H', 2),
             5125: ('I', 4), 5126: ('f', 4)}
WIDTH = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def check_glb(path, metadata):
    blob = path.read_bytes()
    magic, version, length = struct.unpack_from('<III', blob)
    assert (magic, version, length) == (0x46546C67, 2, len(blob)), path
    count, kind = struct.unpack_from('<II', blob, 12)
    assert kind == 0x4E4F534A
    document = json.loads(blob[20:20 + count])
    cursor = 20 + count
    binary_size, binary_kind = struct.unpack_from('<II', blob, cursor)
    assert binary_kind == 0x004E4942 and cursor + 8 + binary_size == len(blob)
    binary = blob[cursor + 8:]
    assert len(document['buffers']) == 1 and 'uri' not in document['buffers'][0]
    assert not document.get('extensionsRequired')

    def values(index):
        accessor = document['accessors'][index]
        assert not accessor.get('sparse'), 'Delivery should not use sparse accessors'
        view = document['bufferViews'][accessor['bufferView']]
        code, size = COMPONENT[accessor['componentType']]
        width = WIDTH[accessor['type']]
        stride = view.get('byteStride', width * size)
        start = view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
        assert accessor['count'] > 0
        assert accessor.get('byteOffset', 0) + (accessor['count'] - 1) * stride + width * size <= view['byteLength']
        result = [struct.unpack_from('<' + code * width, binary, start + i * stride)
                  for i in range(accessor['count'])]
        assert all(math.isfinite(x) for row in result for x in row)
        return result

    parents = {}
    for index, node in enumerate(document['nodes']):
        for child in node.get('children', []):
            assert child not in parents and child != index
            parents[child] = index
    for index in range(len(document['nodes'])):
        seen = set()
        while index in parents:
            assert index not in seen
            seen.add(index)
            index = parents[index]
    assert sum(len(skin['joints']) for skin in document.get('skins', [])) <= 48
    triangles = 0
    for node in document['nodes']:
        if 'mesh' not in node:
            continue
        skin = document['skins'][node['skin']] if 'skin' in node else None
        for primitive in document['meshes'][node['mesh']]['primitives']:
            attributes = primitive['attributes']
            assert primitive.get('mode', 4) == 4 and not primitive.get('targets')
            positions = values(attributes['POSITION'])
            assert len(values(attributes['NORMAL'])) == len(positions)
            indices = [row[0] for row in values(primitive['indices'])]
            assert len(indices) % 3 == 0 and max(indices) < len(positions)
            triangles += len(indices) // 3
            if skin:
                joints = values(attributes['JOINTS_0'])
                weights = values(attributes['WEIGHTS_0'])
                assert len(joints) == len(weights) == len(positions)
                assert 'JOINTS_1' not in attributes
                assert all(0 <= j < len(skin['joints']) for row in joints for j in row)
                assert all(sum(row) > 0 and min(row) >= 0 for row in weights)
        if skin:
            assert len(values(skin['inverseBindMatrices'])) == len(skin['joints'])
    for material in document.get('materials', []):
        assert material.get('alphaMode', 'OPAQUE') == 'OPAQUE'
        assert not any('Texture' in key for key in material)
        assert not any('Texture' in key for key in material.get('pbrMetallicRoughness', {}))
    clips = {clip['name']: clip for clip in metadata['clips']}
    assert set(clips) == {animation['name'] for animation in document.get('animations', [])}
    for animation in document.get('animations', []):
        duration = 0
        targets = set()
        for channel in animation['channels']:
            target = channel['target']
            assert target['path'] in ('translation', 'rotation', 'scale')
            assert 'matrix' not in document['nodes'][target['node']]
            key = (target['node'], target['path'])
            assert key not in targets
            targets.add(key)
            sampler = animation['samplers'][channel['sampler']]
            times = [row[0] for row in values(sampler['input'])]
            assert times[0] >= 0 and all(a < b for a, b in zip(times, times[1:]))
            interpolation = sampler.get('interpolation', 'LINEAR')
            assert interpolation in ('LINEAR', 'STEP', 'CUBICSPLINE')
            output = values(sampler['output'])
            assert len(output) == len(times) * (3 if interpolation == 'CUBICSPLINE' else 1)
            assert len(output[0]) == (4 if target['path'] == 'rotation' else 3)
            duration = max(duration, times[-1])
        assert abs(duration - clips[animation['name']]['duration']) < .001
    return triangles, len(clips)


def check_obj(path):
    positions = normals = triangles = 0
    materials = set()
    active = None
    for raw in path.read_text(encoding='utf-8').splitlines():
        fields = raw.partition('#')[0].split()
        if not fields:
            continue
        if fields[0] == 'mtllib':
            mtl = (path.parent / fields[1]).resolve()
            assert mtl.is_file()
            for line in mtl.read_text().splitlines():
                words = line.split()
                if words and words[0] == 'newmtl':
                    materials.add(words[1])
        elif fields[0] in ('v', 'vn'):
            assert len(fields) == 4 and all(math.isfinite(float(v)) for v in fields[1:])
            positions += fields[0] == 'v'
            normals += fields[0] == 'vn'
        elif fields[0] == 'usemtl':
            active = fields[1]
            assert active in materials
        elif fields[0] == 'f':
            assert len(fields) == 4 and active is not None
            for token in fields[1:]:
                parts = token.split('/')
                assert len(parts) == 3 and parts[2]
                for value, count in ((int(parts[0]), positions), (int(parts[2]), normals)):
                    index = count + value if value < 0 else value - 1
                    assert 0 <= index < count
            triangles += 1
    assert triangles > 0
    return triangles


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--art', type=Path, default=ROOT / '.modeling-dev/Art')
    arguments = parser.parse_args()
    art = arguments.art.resolve()
    manifest = json.loads((art / 'Characters/manifest.json').read_text(encoding='utf-8'))
    catalog = json.loads((art / 'LowPoly/catalog.json').read_text(encoding='utf-8'))
    ids = set()
    triangles = clips = 0
    for entry in manifest['assets']:
        assert entry['id'] not in ids
        ids.add(entry['id'])
        file = art / 'Characters' / entry['glb']
        sidecar = file.with_suffix('.animations.json')
        metadata = json.loads(sidecar.read_text(encoding='utf-8')) if sidecar.is_file() else entry
        mesh_triangles, count = check_glb(file, metadata)
        triangles += mesh_triangles
        clips += count
    static = list(catalog['environment'])
    for slot, entries in catalog['slots'].items():
        assert len({entry['index'] for entry in entries}) == len(entries), slot
        static.extend(entries)
    for entry in static:
        assert entry['name'] not in ids
        ids.add(entry['name'])
        assert check_obj(art / 'LowPoly' / entry['file']) == entry['triangles']
    ns = {'ms': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    for name in ('SimpleGame.vcxproj', 'SimpleGame.vcxproj.filters'):
        project = ET.parse(ROOT / 'SimpleGame' / name)
        for kind, pattern in (('ClCompile', '*.cpp'), ('ClInclude', '*.h')):
            items = [node.attrib['Include'] for node in project.findall('.//ms:' + kind, ns)
                     if 'Include' in node.attrib]
            for source in (ROOT / 'SimpleGame').glob(pattern):
                assert items.count(source.name) == 1, (name, source)
    expected = {
        'cgltf.h': 'e378a21c084bf1f288bb799de827bb26906efb024255f1ecf1705ea13f11c6ec',
        'json.hpp': '9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6',
    }
    for name, digest in expected.items():
        # Git may check out CRLF on Windows; compare the upstream canonical LF content.
        content = (ROOT / 'SimpleGame/Dependencies' / name).read_bytes().replace(b'\r\n', b'\n')
        assert hashlib.sha256(content).hexdigest() == digest
    header = (ROOT / 'SimpleGame/ModelAsset.h').read_text()
    shader = (ROOT / 'SimpleGame/Shaders/Model.vs').read_text()
    assert re.search(r'MaxPalette = 48;', header) and 'u_Palette[48]' in shader
    print(f'Asset contract OK: {len(manifest["assets"])} GLBs, {clips} clips, '
          f'{len(static)} OBJs, {triangles} character triangles; project entries and dependency hashes OK.')


if __name__ == '__main__':
    main()
