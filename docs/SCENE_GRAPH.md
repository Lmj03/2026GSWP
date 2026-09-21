# Actor와 SceneGraph

화면에 배치하는 논리 객체는 `Actor`를 상속하고 `SceneGraph::Create<T>()`로 등록한다. `SceneGraph`가 `unique_ptr`로 수명을 소유하며, 프레임마다 `Update()`와 `Draw()`를 호출한다. OpenGL 버퍼·셰이더·HDR 후처리는 기존 `Renderer`가 담당한다. 타일 청크, 날씨, HUD 내부의 삼각형·빗방울·글리프는 각 Actor가 출력하는 렌더링 요소다.

## 현재 장면

```text
world
  player — player/attack
  streamed
    terrain/<청크 좌표>
    building/<ID> — reflection
    citizen/<ID>
  authored
    enemy/<ID> — loot, warning
    base — ground
    supply
    drone
screen
  weather
  hud
  base/menu
```

이름의 `/`는 ID 표기다. 실제 부모 관계는 `Create`의 두 번째 인자 또는 `Reparent`로 지정한다.

## 공통 제어

- `LocalPosition`, `SetLocalPosition`: 부모 기준 XY 위치.
- `WorldPosition`, `SetWorldPosition`: 부모의 XY 이동을 합산한 장면 좌표. 회전·스케일 상속은 아직 지원하지 않는다.
- `enabled`: 자신과 자식의 갱신·표시를 중단한다.
- `visible`: 자신과 자식의 표시만 중단한다.
- `tickEnabled`, `renderEnabled`: 해당 Actor의 갱신·그리기 참여 여부.
- `Reparent`: 순환 부모 관계를 거부하며 기본적으로 기존 장면 위치를 유지한다.
- `Destroy`: 자식까지 삭제 예약한다. 갱신 중에는 실제 해제를 미루고, `FlushDestroyed` 또는 다음 갱신에서 해제한다. 그리기 중에도 즉시 해제하지 않는다.

ID는 비어 있지 않고 유일해야 한다. 삭제 예약 중인 ID는 해제 전 재사용할 수 없다. 순회 중 생성된 Actor는 다음 갱신부터 참여한다. `Draw`는 표시 전용으로 사용하고 게임 상태 변경은 `Update`에서 처리한다. Actor 참조는 소유 그래프에서 제거되면 더 이상 사용할 수 없다.

`ActorView`는 건물·주민·적을 빠르게 조회하는 비소유 인덱스다. 장면의 제거 경로에서는 인덱스를 먼저 갱신하고 그래프에서 해제한다. 기존 게임 Actor를 임의로 직접 삭제하는 대신 스트리밍 갱신 또는 적 재등록 경로를 사용해야 한다. 플레이어와 장면 루트는 장면 전체 수명 동안 유지한다.

## 갱신·표시 순서

거점(0) → 플레이어와 타이머(10) → 적(20) → 전리품(40) → 주민(50) 순서로 갱신한다. 동일 순서는 생성 순서로 결정한다. 지도·거점·여러 페이지 대화의 게임 정지 조건을 각 게임 Actor가 공유한다.

표시는 Terrain → Ground → World → Air → Weather → Interface → Modal 순이다. 같은 레이어는 `x + y + depthOffset`으로 정렬하고, 깊이가 같으면 `drawOrder`와 생성 순서를 따른다. 건물 깊이에는 바닥 폭과 깊이를 더한다. Interface에 진입할 때 `BeginOverlay`를 호출하므로 월드 HDR 후처리와 UI 분리가 유지된다.

## 무한 월드 좌표

월드 원점은 기존 64비트 청크 좌표로 유지한다. `WorldPosition`은 거대한 절대 좌표가 아니라 현재 원점 근처의 작은 장면 좌표다. `streamed`의 객체는 원점 이동 시 다시 배치하고, `authored` 루트는 원점만큼 반대로 이동한다. 고정 레벨의 적·거점·보급 상자는 수제 배치 좌표를 유지한다. 해당 게임 로직은 이 좌표 규칙을 전제로 하므로 이 노드를 임의의 좌표계로 옮기려면 상호작용·전투 좌표 변환도 함께 변경해야 한다.

스트리밍은 좌표에서 만든 ID로 기존 Actor를 재사용하고, 활성 범위에서 벗어난 Actor와 그 자식을 삭제한다. 방문한 모든 청크를 계속 보관하지 않는다. 수제 배치와 주민의 도움 기억 데이터는 기존 방식으로 유지한다.

## 파일 책임

| 파일 | 책임 |
|---|---|
| `Actor.*`, `SceneGraph.*` | 게임 규칙에 의존하지 않는 객체·계층·수명·순회 |
| `GameActors.*` | 구체 Actor, 건물·주민·적의 상태 소유, 갱신·표시 연결 |
| `SceneActors.cpp` | 장면 구성, Actor 등록, 좌표 루트 동기화 |
| `LevelOne.cpp` | 레벨 1 데이터 로드, 목표·상호작용 |
| `CharacterBehavior.cpp`, `Combat.cpp` | 이동·주민 행동, 공격·적 행동·부활 |
| `Progression.cpp`, `Loot.cpp`, `PlayerBase.cpp` | 성장, 자동 습득, 거점·제작·창고 |
| `WorldRendering.cpp`, `CombatRendering.cpp`, `InterfaceRendering.cpp` | 각 Actor에서 사용하는 표시 함수 |
| `WorldStreaming.cpp` | 청크 생성·원점 이동·Actor 재사용 |

공유 진행 상태와 충돌·카메라·지역화 서비스는 `CityScene`에 남아 있다. 동작 파일 분리는 별도의 독립 게임 시스템으로 완전히 추출했다는 의미는 아니다. 새 객체는 구체 Actor에 상태와 `Update`/`Draw`를 구현하여 추가하며, 기존 레벨의 종류별 렌더 분기문을 늘릴 필요가 없다.

```cpp
// CityScene의 장면 구성 코드에서 등록한다.
auto& prop = m_sceneGraph.Create<MyPropActor>("prop/terminal", "streamed", *this);
prop.SetLocalPosition({3.5f, 4.5f});
```

이번 변경은 빌드·실행하지 않았다. 사용자 빌드에서는 청크 경계 왕복, 전투와 전리품, 사망·부활, F2 초기화, 거점 휴식, 지도·대화 정지, 건물 가림과 HDR/UI 순서를 확인한다.
