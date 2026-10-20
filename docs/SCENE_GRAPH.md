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
    enemy/generated/<청크>/mob/<번호> — loot, warning
  authored
    enemy/story_heir — loot, warning
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

거점(0) → 스토리 제어(5) → 플레이어와 타이머(10) → 적(20) → 전리품(40) → 주민(50) 순서로 갱신한다. 동일 순서는 생성 순서로 결정한다. 지도·거점·여러 페이지 대화의 게임 정지 조건을 각 게임 Actor가 공유한다.

표시는 Terrain → Ground → World → Air → Weather → Interface → Modal 순이다. 같은 레이어는 `x + y + depthOffset`으로 정렬하고, 깊이가 같으면 `drawOrder`와 생성 순서를 따른다. 건물 깊이에는 바닥 폭과 깊이를 더한다. Interface에 진입할 때 `BeginOverlay`를 호출하므로 월드 HDR 후처리와 UI 분리가 유지된다.

## 무한 월드 좌표

월드 원점은 기존 64비트 청크 좌표로 유지한다. `WorldPosition`은 거대한 절대 좌표가 아니라 현재 원점 근처의 작은 장면 좌표다. `streamed`의 객체는 원점 이동 시 다시 배치하고, `authored` 루트는 원점만큼 반대로 이동한다. 스토리 가해자·거점·보급 상자는 수제 배치 좌표를 유지한다. 일반 몹은 streamed 아래 생성하며 위치·귀환점·공격 예고를 작은 장면 좌표로 관리하고 원점 이동량을 함께 차감한다. EnemyLocal은 이 두 좌표계를 명시적으로 구분한다. 해당 게임 로직은 이 좌표 규칙을 전제로 하므로 이 노드를 임의의 좌표계로 옮기려면 상호작용·전투 좌표 변환도 함께 변경해야 한다.

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
| `WorldPopulation.cpp` | 일반 몹 생성·재생성·좌표 변환·로드 해제 기억 |
| `InterfaceInput.cpp` | 마우스로 대화·거점·지도·기록·회복제 조작 |

공유 진행 상태와 충돌·카메라·지역화 서비스는 `CityScene`에 남아 있다. 동작 파일 분리는 별도의 독립 게임 시스템으로 완전히 추출했다는 의미는 아니다. 새 객체는 구체 Actor에 상태와 `Update`/`Draw`를 구현하여 추가하며, 기존 레벨의 종류별 렌더 분기문을 늘릴 필요가 없다.

```cpp
// CityScene의 장면 구성 코드에서 등록한다.
auto& prop = m_sceneGraph.Create<MyPropActor>("prop/terminal", "streamed", *this);
prop.SetLocalPosition({3.5f, 4.5f});
```

이번 변경은 빌드·실행하지 않았다. 사용자 빌드에서는 청크 경계 왕복, 전투와 전리품, 사망·부활, F2 초기화, 거점 휴식, 지도·대화 정지, 건물 가림과 HDR/UI 순서를 확인한다.


## 1막과 NPC 사회 확장 (2026-10-13)

`story/controller`는 독립 루트의 StoryActor로 순서 5에서 사건과 단계별 활성화를 갱신한다. 사건 거점은 `authored` 아래 StoryNodeActor, 피해자·경호원·가족·증인·저항자는 기존 CitizenActor, 가해자는 EnemyActor로 등록한다. 대화·기록은 `screen` 아래 DialogueActor가 Modal 레이어에서 출력한다.

CitizenActor는 변경된 행동·건강·악명·신뢰 데이터를 소유 레코드에 반영한다. 같은 Actor를 청크 갱신에서 재사용할 때 공격 예고·행동 타이머를 보존하고, 범위 밖으로 해제된 NPC의 의미 있는 이력은 ID별 메모리에서 복원한다. 상세 범위와 조작은 [ACT_ONE_IMPLEMENTATION.md](ACT_ONE_IMPLEMENTATION.md)를 따른다.

일반 몹의 신규 생성은 SceneGraph 갱신이 끝난 뒤 실행한다. 삭제 예약 ID를 갱신 도중 다시 만들지 않으며, 생성 시 Actor와 전리품·공격 예고 자식을 함께 등록한다. 훈련 진행도는 현재 Actor 수에 의존하지 않는 누적 카운터로 유지한다.

## 마을 명부와 화면 밖 이동 (2026-10-20)

`TownPopulation.cpp`는 7개 마을의 총 490명 명부를 유지한다. 표시 범위의 NPC는 SceneGraph 소유 CitizenActor가 상태를 갱신하고, 갱신 후 명부에 동기화한다. 범위 밖에서는 명부의 제한된 도로 경로와 대기만 갱신한다. NPC의 실체를 두 개 생성하지 않는다.

명부의 위치·귀환점과 route 배열은 일곱 마을 범위 안의 절대 좌표이며, Actor 위치·귀환점·공격 예고는 기존 작은 장면 좌표다. `TownScenePoint`에서 원점 범위를 먼저 확인한 뒤 변환하고, 경로 배열 자체에는 원점 이동량을 누적하지 않는다. 로드 범위 밖의 NPC Actor를 해제하기 전 이력을 동기화한다. 일반 NPC를 청크 생성 결과에 넣던 방식은 제거했다.

주민 Actor 추가·해제는 SceneGraph 갱신이 끝난 뒤 수행한다. `GameActors.cpp`의 CitizenActor는 `UpdateNpc`에서 변경한 체류 지점을 부모 좌표계로 환산해 보존하므로 마을 방문 후 이전 집으로 잘못 돌아가지 않는다. 지형보다 한 청크 안쪽을 Actor 범위로 사용해 여행자가 지형의 이동 한계에서 멈추지 않게 한다.
