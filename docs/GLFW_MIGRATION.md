# FreeGLUT → GLFW 전환

작성일: 2026-11-03

## 구현과 검사 범위

창·컨텍스트·입력·이벤트 루프를 GLFW 3.4로 교체했다. 기존 C++14 / OpenGL 3.3 core,
GLEW, Renderer, SceneGraph, 게임 규칙과 HDR 후처리를 유지한다. 스왑 함수 외의 렌더링 경로를 바꾸지 않았다.

사용자 작업 원칙에 따라 **빌드·실행·실제 FPS 측정은 하지 않았다**. 이 문서는 코드 구조와
의존성 검사 결과를 기록하며 성능 유지가 실측으로 확인되었다는 의미는 아니다.

## 다른 PC에서 빌드

Windows PC의 Visual Studio 2022에 C++ 데스크톱 개발 도구와 Windows 10/11 SDK가 필요하다.
`SimpleGame.sln`을 열고 Debug/Release 및 x86/x64 중 원하는 구성을 선택한다.
솔루션의 x86은 프로젝트의 Win32에 대응한다.

GLFW의 공식 소스 배포본 전체와 라이선스는 `SimpleGame/Dependencies/GLFW/glfw-3.4`에 있다.
전용 `GLFW.vcxproj`는 솔루션과 게임 프로젝트 양쪽에서 참조한다. GLFW를 먼저 소스로 빌드해
현재 아키텍처와 구성에 맞는 정적 라이브러리를 자동으로 연결한다. CMake 실행, 패키지 관리자,
네트워크 다운로드, PC별 라이브러리 경로 지정이나 GLFW DLL 설치는 필요하지 않다.

게임과 GLFW 모두 Debug는 /MDd, Release는 /MD를 사용한다. GLFW의 Windows 공통·WGL·null
백엔드에 필요한 21개 C 파일을 공식 CMake 목록에 맞춰 등록했다. 시스템 OpenGL 링크도 네 구성에 명시했다.
GLFW 빌드 산출물은 `Dependencies/GLFW/build/<플랫폼>/<구성>`에 분리해 놓고 커밋에서 제외한다.

GLEW는 기존 방식을 유지한다. 기존에 추적하던 DLL을 `Dependencies/Runtime/Win32`와 `x64`로
복사해 아키텍처별 원본으로 관리하며, 빌드가 끝나면 선택한 `$(OutDir)`에 자동 복사한다.
Shaders와 Data도 기존처럼 복사한다. 기존 FreeGLUT 파일은 새 소스·링크 설정에서 참조하지 않는다.
현재 게임의 GDI 글꼴·WinHTTP 등 Windows 의존성까지 다른 OS로 이식한 작업은 아니다.

## 창과 입력

| 기존 | 변경 |
|---|---|
| FreeGLUT 생성·닫기 콜백 | GLFW 창과 명시적인 수명 관리 |
| `glutMainLoop`와 16ms 재등록 타이머 | 하나의 GLFW 이벤트·갱신·표시 루프 |
| 키보드/특수키 콜백 | GLFW 물리 키 → 기존 게임 입력 값 변환 |
| Shift·WASD의 Windows 전역 키 조회 | GLFW press/release 상태만 사용 |
| `glutSwapBuffers` | `glfwSwapBuffers`를 기존 프로파일러 구간에서 측정 |
| 창 크기 기반 렌더 크기 | framebuffer 픽셀 크기, 커서는 창 좌표에 맞춰 별도 변환 |

두 Shift 키를 독립적으로 추적하며 반복 키 이벤트는 재실행하지 않는다. Space를 한 프레임보다
짧게 눌러도 다음 갱신에 한 번 전달한다. 포커스 상실·최소화 시 눌린 키·미처리 클릭을 지워
복귀 후 이동/공격이 붙어 있는 현상을 막는다. 커서 클릭은 정규화한 좌표를 최대 8개 보관해
같은 이벤트 처리 구간의 framebuffer 크기 변경을 적용한 뒤 UI로 전달한다.

휠의 소수 이동량은 누적하고 한 단계가 모이면 확대/축소한다. 창 크기 콜백에서는 새 크기만
기록한다. 실제 렌더 타깃 재생성은 다음 활성 프레임에 한 번만 수행하고 0×0 크기로 만들지 않는다.
종료 시 모니터링 중단 → GPU 자원 해제 → 창 파괴 → GLFW 종료 순서를 보장한다.

## 성능을 위한 프레임 스케줄링

설정 파일은 `Data/Rendering/window.cfg`이며 변경 후 다시 실행한다.

| 설정 | 기본값 | 의미 |
|---|---:|---|
| `frame_interval_ms` | 16 | 기존과 같은 명목 갱신 주기. 0이면 CPU 제한 없음 |
| `swap_interval` | 0 | 0: 드라이버에 VSync 해제를 요청. 1: VSync 요청 |
| `background_interval_ms` | 100 | 비활성·최소화 시 이벤트/모니터링 대기 상한 |

- 활성 프레임 시작 시각 + 설정 주기를 마감 시각으로 정한다. 갱신·렌더·스왑 비용을 뺀
  남은 시간만 기다린다. 렌더링 후 고정 16ms를 추가해 FPS가 떨어지는 구조를 피했다.
- VSync=1이면 별도 CPU 마감 대기를 하지 않는다. VSync와 프레임 대기를 중첩해
  주사율의 절반으로 떨어지는 상황을 피한다. 드라이버 강제 설정까지 제어하거나 실제 VSync 상태를 조회하지는 않는다.
- Windows 고해상도 waitable timer를 한 번 생성해 재사용한다. 대기 중 바쁜 반복을 하지 않으며
  전역 타이머 해상도를 변경하지 않는다. Windows 10 1803 이전 또는 생성 실패 시 일반 타이머/표준 sleep으로 복귀하고
  실제 선택된 모드를 로그에 남긴다. 폴백 환경에서는 대기 정밀도 차이가 있을 수 있다.
- 입력 이벤트는 활성 프레임마다 한 번 처리한다. CPU 제한이 있는 동안 추가 입력 지연은
  통상 한 주기 이내이며 긴 GPU/OS 지연 시에는 늘어날 수 있다.
- 포커스 상실·최소화 때 갱신과 표시를 쉬고 `glfwWaitEventsTimeout`으로 대기한다.
  JSON 모니터링과 이벤트 처리는 유지한다. 복귀 시 정지 시간을 dt에 넣거나 몰아서 갱신하지 않는다.
- 기존 dt 상한 50ms를 유지한다. 프레임마다 메시·버퍼·글꼴·모델을 다시 만드는 코드는 추가하지 않았다.

GPU에 제출되는 월드·모델·UI·후처리 경로와 해상도는 전환 전과 동일하다.
비활성 창은 렌더링을 중단하므로 이전과 프레임 수가 다르다. 활성 상태의 같은 조건으로 비교해야 한다.

## 로그와 성능 비교

기존 CSV schema 2와 FPS·드로 수·업로드량·update/render/swap 측정을 유지한다.
대기와 이벤트 처리 시간을 렌더 CPU 시간으로 합산하지 않는다.

시작 시 콘솔과 이벤트 로그에 GLFW 버전, 요청한 swap interval, 유효 CPU 주기,
실제 프레임버퍼 크기, 타이머 모드, GL 공급자·렌더러·버전을 기록한다.
매초 이벤트 로그의 `window_stats`에는 프레임 수, 이벤트 처리 시간, 대기 시간,
마감 초과 횟수와 dt 제한 횟수, 포커스·최소화 상태를 남긴다.

사용자 측에서는 전환 전 `c03b58f`와 전환 후를 같은 PC·Release x64·해상도·이동 경로·후처리·창 활성 상태로 실행해
각각 로그를 저장한다. 처음 5초 이상 워밍업을 제외한다. 포커스 전환 구간은 측정 범위에서 제외한다.

```powershell
python tools/analyze_render_profile.py AFTER.csv --baseline BEFORE.csv --warmup 5 --note "같은 경로와 설정"
```

평균 FPS뿐 아니라 프레임 시간 p95/p99, update_ms, render_cpu_ms, swap_ms, 드로 수와 업로드량을 비교한다.
드로 수가 같아도 swap_ms가 늘면 VSync·드라이버·주사율을, 대기 시간이 늘면 타이머 모드와 설정을 확인한다.
마감 초과 시 별도 대기를 더하지 않으므로 렌더가 16ms를 넘는 경우 실제 렌더 병목을 먼저 살핀다.
실제 수치와 성능 판정은 아직 없다.

## 정적 검사와 후속 사용자 확인

`tools/validate_glfw_migration.py`로 배포본 362개 파일의 해시, Windows C 파일 목록,
네 가지 구성과 CRT 일치, 솔루션·프로젝트 참조, 시스템 라이브러리, GLEW DLL의 PE 아키텍처,
공개 GLFW API 선언과 기존 FreeGLUT 호출 제거를 검사한다. 컴파일·실행 테스트를 대신하지 않는다.

사용자 빌드 후 WASD·양쪽 Shift, 빠른 Space 탭/연타, E·거점·기록·지도·Esc, F2/F5~F9,
마우스·휠, 다른 DPI 모니터로 이동, 창 크기 변경, 최소화/복귀, 창 X/초기화 실패 시 종료를 확인한다.
성능 비교에서는 모델 첫 업로드나 최초 글꼴 생성 같은 준비 비용도 워밍업과 분리한다.

참고: [GLFW 시작 안내](https://www.glfw.org/docs/3.4/quick.html),
[입력 안내](https://www.glfw.org/docs/3.4/input_guide.html),
[컴파일 안내](https://www.glfw.org/docs/3.4/compile_guide.html),
[Windows 고해상도 타이머](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw).
