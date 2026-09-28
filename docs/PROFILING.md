# 렌더링 로그와 분석 절차

전체 변경 보고서: [2026-09-22-draw-call-report.md](performance/2026-09-22-draw-call-report.md).

## 출력과 저장

콘솔에 약 1초마다 FPS, 최근/평균/최소/최대 draw call, World/UI/Post 호출, 인스턴스 수와 업로드 KiB를 출력한다. API 호출은 `glDrawArrays` 또는 `glDrawArraysInstanced`가 실제 실행될 때 각각 한 번 집계한다. `glDrawBuffer`는 상태 설정이므로 제외한다.

실행 파일 옆 `Logs/render-<UTC 날짜·시간>-<PID>.csv`에 매 프레임 자료를 저장하고, 같은 이름의 `.log`에 설정·GPU·캐시 이벤트와 콘솔 요약을 저장한다. 폴더 생성에 실패하면 `%LOCALAPPDATA%/NeonVeil/Logs`를 시도하며 실제 경로를 콘솔에 출력한다. 파일 쓰기가 실패해도 게임과 콘솔 집계는 유지한다. 로그를 자동 삭제하지 않으므로 장기 측정 후 보관·정리는 사용자가 한다.

CSV는 메모리에 모아 약 1초마다 저장하고 정상 종료 시 남은 행을 저장한다. 비정상 종료 시 마지막 미저장 구간은 유실될 수 있다. 현재 스키마는 2이며 별도의 GPU 동기화나 `glFinish`를 삽입하지 않는다.

## CSV 열

| 열 | 의미 |
|---|---|
| frame, elapsed_s | 프레임 번호, 프로파일러 시작 후 실제 시간 |
| frame_interval_ms | 이전 프레임 버퍼 교환 완료부터 현재 완료까지. 첫 행은 첫 Display 시작 기준 |
| update_calls, update_ms | 이전 Display 이후 수행한 게임 Update 횟수와 CPU 경과 시간 합계. 포커스 해제 시 0일 수 있음 |
| render_cpu_ms | `city.Draw`의 CPU 경과 시간. submit/post 시간 포함 |
| submit_cpu_ms | 인스턴스 버퍼 업로드와 배치 draw 제출의 CPU 시간 |
| post_cpu_ms | 후처리 Present의 CPU 시간 |
| swap_ms | `glutSwapBuffers` 경과 시간. VSync·드라이버 대기가 포함될 수 있음 |
| previous_log_ms | 직전 프레임의 행 생성·콘솔·파일 기록 비용. 현재 frame interval에 포함될 수 있음 |
| draw_total, draw_world, draw_overlay, draw_post | 전체·월드·UI·후처리의 실제 API 호출 수 |
| instanced_draws, instances | 인스턴스 draw 호출 수와 제출된 인스턴스 수 |
| submitted_vertices | 인스턴스 수를 곱한 정점 수 + 후처리 정점 수. 퇴화 삼각형 정점도 포함 |
| instance_upload_bytes | 해당 프레임에 전송한 인스턴스 데이터. GPU 버퍼 전체 예약 용량과는 다름 |
| texture_binds | 월드/UI 배치 제출 중 텍스처 바인딩 횟수. 후처리·폰트 업로드 바인딩 제외 |
| texture_splits, capacity_splits | 텍스처 슬롯 또는 인스턴스 상한 때문에 생긴 추가 배치 수 |
| culled | Queue에서 화면 밖 또는 알파 0으로 제거한 요청 수. Actor 단계에서 제거된 수는 제외 |
| font_hits, font_misses, font_upload_bytes | 문자열 이미지 메모리 캐시 적중·미적중과 신규 coverage 텍스처 업로드 |
| legacy_draw_estimate | 같은 요청을 기존 연속 텍스처 배치 방식으로 처리할 때의 월드+UI 호출 추정. 컬링 전, 후처리 제외 |
| legacy_vertices | 같은 요청을 기존 삼각형 생성 방식으로 처리할 때의 정점 수 추정. 컬링 전, 후처리 제외 |

`render_cpu_ms`에 `submit_cpu_ms`와 `post_cpu_ms`를 다시 더하지 않는다. `frame_interval_ms`에는 CPU 측정 범위 밖의 타이머·스케줄링·이전 로그 비용도 포함될 수 있다. 현재 시간 측정은 GPU 타이머가 아니므로 GPU 부하를 단정하지 않는다.

## 이벤트 로그

렌더러/로그 버전, Debug/Release, GPU/드라이버 문자열, 배치 슬롯·용량·인스턴스 크기, 메시 캐시 상태, 정적 메시 GPU 업로드, 인스턴스 버퍼 용량 변경, 뷰포트와 주요 후처리 설정 변경을 기록한다. 저장소 커밋과 실제 조작 경로는 분석 명령의 `--note`에 함께 남긴다.

## 분석 도구

Python 표준 라이브러리만 사용한다. 게임 빌드나 실행을 수행하는 도구가 아니다.

```powershell
python tools/analyze_render_profile.py x64/Release/Logs/render-실제파일명.csv --note "커밋 SHA / 시작 거점 정지 / 1280x800 / HDR 기본" --output docs/performance/runs/거점-정지.md
```

기본적으로 첫 프레임과 첫 프레임 후 5초를 제외한다. `--warmup 0`으로 준비 구간도 포함할 수 있다. `--start 10 --end 40`은 첫 프레임을 기준으로 10~40초를 선택한다. 후처리 설정이나 장면을 바꾼 구간은 이벤트 로그와 맞춰 별도로 분석한다.

```powershell
python tools/analyze_render_profile.py 새측정.csv --baseline 기준측정.csv --warmup 5 --note "동일 장면 / 슬롯 8 대 슬롯 1" --output docs/performance/runs/배치비교.md
```

프레임 시간의 평균·중앙값·P95·P99·최대와 호출·전송·CPU 지표를 정리한다. FPS는 프레임 간 시간 합계로 계산하며 순간 FPS의 단순 평균을 쓰지 않는다. `--baseline`은 동일 스키마 CSV가 필요하고, 이전 콘솔 로그 6줄은 비교 CSV로 간주하지 않는다. 실행 조건이 다르면 변화율만으로 최적화 효과를 판단하지 않는다.

## 비교 설정

`Data/Rendering/batching.cfg`를 수정한 후 게임을 다시 시작한다. 실행 파일 옆 리소스를 우선 읽으므로 소스 폴더 파일을 수정했다면 사용자 빌드로 복사하거나 실행 폴더의 설정 파일을 직접 수정한다. F9는 기존 후처리 설정만 다시 읽는다.

- `texture_slots`: 1~8, 기본 8. 1/8 비교는 새 인스턴싱 렌더러 내부의 텍스처 배치 효과만 비교한다.
- `max_instances`: 256~65536, 기본 16384. 상한 도달 시 순서를 유지한 채 다음 배치로 넘긴다.
- `culling`: 0/1, 기본 1. Renderer Queue의 보수적 화면 컬링을 켜거나 끈다.

## 현재 검증 상태

변경 후 FPS·GPU 성능·표시 결과는 아직 측정하지 않았다. 사용자 빌드와 실행에서 확보한 CSV/이벤트 로그를 바탕으로 `docs/performance/runs` 아래 후속 보고서를 추가한다.
