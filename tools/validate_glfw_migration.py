"""Validate GLFW packaging and the Windows build graph without compiling/running the game."""
from pathlib import Path
import hashlib
import re
import struct
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / 'SimpleGame'
GLFW = GAME / 'Dependencies/GLFW'
UPSTREAM = GLFW / 'glfw-3.4'
NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
GUID = '{58E6C143-B617-49E9-AF76-65BD57767403}'


def elements(tree, name):
    return tree.findall('.//m:' + name, NS)


def validate():
    recorded = {}
    for line in (GLFW / 'upstream.sha256').read_text(encoding='ascii').splitlines():
        digest, name = line.split('  ', 1)
        assert name not in recorded and not Path(name).is_absolute() and '..' not in Path(name).parts
        recorded[name] = digest
    actual = {file.relative_to(UPSTREAM).as_posix() for file in UPSTREAM.rglob('*') if file.is_file()}
    assert actual == set(recorded), ('Incomplete GLFW distribution', set(recorded) - actual)
    for name, digest in recorded.items():
        assert hashlib.sha256((UPSTREAM / name).read_bytes()).hexdigest() == digest, name
    assert (UPSTREAM / 'LICENSE.md').is_file()
    assert 'SimpleGame/Dependencies/GLFW/glfw-3.4/** -text' in (ROOT / '.gitattributes').read_text(encoding='utf-8')

    glfw = ET.parse(GLFW / 'GLFW.vcxproj')
    game = ET.parse(GAME / 'SimpleGame.vcxproj')
    # Windows manual build source set from the vendored official src/CMakeLists.txt.
    expected_sources = {
        'context.c', 'init.c', 'input.c', 'monitor.c', 'platform.c', 'vulkan.c', 'window.c',
        'egl_context.c', 'osmesa_context.c', 'null_init.c', 'null_monitor.c', 'null_window.c',
        'null_joystick.c', 'win32_module.c', 'win32_time.c', 'win32_thread.c', 'win32_init.c',
        'win32_joystick.c', 'win32_monitor.c', 'win32_window.c', 'wgl_context.c',
    }
    sources = [item.attrib['Include'] for item in elements(glfw, 'ClCompile') if 'Include' in item.attrib]
    assert len(sources) == len(set(sources)) == len(expected_sources)
    assert {Path(name.replace('\\', '/')).name for name in sources} == expected_sources
    for name in sources:
        assert (GLFW / name.replace('\\', '/')).is_file(), name
    project_text = (GLFW / 'GLFW.vcxproj').read_text(encoding='utf-8')
    assert 'StaticLibrary' in project_text and 'CompileAsC' in project_text and 'NotUsing' in project_text
    assert '_GLFW_WIN32' in project_text
    assert not any(option in project_text for option in ('GLFW_DLL', '_GLFW_BUILD_DLL', 'GLFW_INCLUDE_NONE'))
    assert not re.search(r'\b[A-Za-z]:[\\/]', project_text), 'Machine-local library build path'
    assert not re.search(r'\b[A-Za-z]:[\\/]', (GAME / 'SimpleGame.vcxproj').read_text(encoding='utf-8'))
    for configuration in ('Debug|Win32', 'Release|Win32', 'Debug|x64', 'Release|x64'):
        for tree in (game, glfw):
            assert any(item.attrib.get('Include') == configuration for item in elements(tree, 'ProjectConfiguration'))
        config = next(item for item in elements(game, 'ItemDefinitionGroup')
                      if configuration in item.attrib.get('Condition', ''))
        libs = config.find('m:Link/m:AdditionalDependencies', NS).text
        assert 'freeglut' not in libs.lower()
        assert all(name in libs for name in ('opengl32.lib', 'glew32.lib', 'winhttp.lib', 'gdi32.lib', 'user32.lib'))
        directories = config.find('m:Link/m:AdditionalLibraryDirectories', NS).text
        assert '$(ProjectDir)' in directories
        runtime = config.find('m:ClCompile/m:RuntimeLibrary', NS).text
        assert runtime == ('MultiThreadedDebugDLL' if configuration.startswith('Debug') else 'MultiThreadedDLL')
    reference = elements(game, 'ProjectReference')
    assert len(reference) == 1 and reference[0].find('m:Project', NS).text == GUID
    assert reference[0].find('m:LinkLibraryDependencies', NS).text == 'true'
    assert (GAME / reference[0].attrib['Include'].replace('\\', '/')).resolve() == (GLFW / 'GLFW.vcxproj').resolve()
    assert len(elements(glfw, 'ProjectGuid')) == 1 and elements(glfw, 'ProjectGuid')[0].text == GUID
    solution = (ROOT / 'SimpleGame.sln').read_text(encoding='utf-8-sig')
    assert 'SimpleGame\\Dependencies\\GLFW\\GLFW.vcxproj' in solution
    for config in ('Debug', 'Release'):
        for platform, target in (('x64', 'x64'), ('x86', 'Win32')):
            assert f'{GUID}.{config}|{platform}.Build.0 = {config}|{target}' in solution

    # Runtime DLLs are included per architecture and copied even with a custom OutDir.
    for platform, machine in (('Win32', 0x014C), ('x64', 0x8664)):
        binary = (GAME / 'Dependencies/Runtime' / platform / 'glew32.dll').read_bytes()
        assert binary[:2] == b'MZ'
        pe = struct.unpack_from('<I', binary, 0x3C)[0]
        assert binary[pe:pe + 4] == b'PE\0\0'
        assert struct.unpack_from('<H', binary, pe + 4)[0] == machine, platform
    game_text = (GAME / 'SimpleGame.vcxproj').read_text(encoding='utf-8')
    assert 'Dependencies\\Runtime\\$(Platform)\\glew32.dll' in game_text
    assert '<Copy SourceFiles="@(GlewRuntime)" DestinationFolder="$(OutDir)"' in game_text

    for name in ('SimpleGame.vcxproj', 'SimpleGame.vcxproj.filters'):
        project = ET.parse(GAME / name)
        for kind, extension in (('ClCompile', 'cpp'), ('ClInclude', 'h')):
            items = [item.attrib['Include'] for item in elements(project, kind) if 'Include' in item.attrib]
            for file in GAME.glob('*.' + extension):
                assert items.count(file.name) == 1, (name, file.name)
    active = '\n'.join(file.read_text(encoding='utf-8-sig') for file in GAME.glob('*.cpp'))
    assert not re.search(r'\bglut[A-Za-z0-9_]*\s*\(', active)
    assert not any(old in active for old in ('Dependencies/freeglut.h', 'GetAsyncKeyState', 'GetForegroundWindow'))
    declarations = (UPSTREAM / 'include/GLFW/glfw3.h').read_text(encoding='utf-8')
    calls = set(re.findall(r'\b(glfw\w+)\s*\(', active))
    for call in calls:
        assert re.search(r'\b' + call + r'\s*\(', declarations), call
    settings = {}
    for line in (GAME / 'Data/Rendering/window.cfg').read_text(encoding='utf-8').splitlines():
        fields = line.partition('#')[0].split()
        if fields:
            assert len(fields) == 2 and fields[0] not in settings
            settings[fields[0]] = int(fields[1])
    assert 0 <= settings['frame_interval_ms'] <= 1000
    assert settings['swap_interval'] in (0, 1)
    assert 20 <= settings['background_interval_ms'] <= 1000
    print(f'GLFW static validation OK: {len(recorded)} upstream files, {len(sources)} Windows C sources, '
          f'4 build configurations, 2 runtime architectures, {len(calls)} public GLFW calls.')
    print('No compiler, executable, graphics context or performance benchmark was invoked.')


if __name__ == '__main__':
    validate()
