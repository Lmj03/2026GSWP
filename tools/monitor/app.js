"use strict";

const byId = (id) => document.getElementById(id);
let latest = null;
let selectedNpc = null;
let villageNames = new Map();
const filters = ["search", "village-filter", "job-filter", "activity-filter", "state-filter"];
const fmt = (value, digits = 0) => value == null ? "—" : Number(value).toLocaleString("ko-KR", { maximumFractionDigits: digits });
const put = (id, value) => { byId(id).textContent = value; };

function element(tag, value, className) {
    const node = document.createElement(tag);
    if (value != null) node.textContent = value;
    if (className) node.className = className;
    return node;
}

function badge(value, kind = "") {
    return element("span", value, `badge ${kind}`);
}

function coordinates(entity) {
    const p = entity.position;
    return `(${p.chunk_x}, ${p.chunk_y}) · ${fmt(p.local_x, 2)}, ${fmt(p.local_y, 2)}`;
}

function options(id, values, all) {
    const select = byId(id);
    const selected = select.value;
    const nodes = [new Option(all, "")];
    for (const [value, label] of values) nodes.push(new Option(label, String(value)));
    select.replaceChildren(...nodes);
    if (values.some(([value]) => String(value) === selected)) select.value = selected;
}

function renderVillages(summary) {
    const fragment = document.createDocumentFragment();
    for (const v of summary.villages) {
        if (!v.loaded_chunks && v.kind === "roads" && !v.population) continue;
        const button = element("button", null, "village");
        button.type = "button";
        button.classList.toggle("selected", v.id === byId("village-filter").value);
        const heading = element("div", null, "village-title");
        heading.append(element("strong", v.name), badge(v.status, v.status === "위험" ? "danger" : v.status === "불안" ? "warn" : v.status === "인구 미관측" ? "off" : ""));
        button.append(heading);
        const details = [
            v.kind === "roads" ? `도로 · 외곽 ${v.loaded_chunks} 청크 관측` : `${v.loaded_chunks} / ${v.expected_chunks} 청크 관측 · ${v.partial ? "일부 로드" : "전체 로드"}`,
            ...(v.kind === "town" ? [`치안 ${v.security} / 100 · 초기 적대 인구 ${v.initial_hostiles}`, `인구 목표 ${v.target_population} · 현지 생존 ${v.population} · 적대 ${v.hostile_population}`, `마을 간 이동 ${v.travelling_population}명 · 명부 기준`] : []),
            `로드 NPC 활동 ${v.npc_active} · 생존 ${v.npc_living} · 적대 ${v.npc_hostile} · 제압/사망 ${v.npc_incapacitated}`,
            `몹 전투 가능 ${v.mob_combat} · 생존 ${v.mob_living} · 제압 ${v.mob_defeated}`,
            `로드 NPC 평균 악명 ${fmt(v.avg_notoriety, 1)} · 신뢰 ${fmt(v.avg_trust, 1)} · 체력 ${fmt(v.avg_health, 1)}%`,
            `직업: ${Object.entries(v.jobs).map(([name, count]) => `${name} ${count}`).join(" / ") || "관측 없음"}`,
            `행동: ${Object.entries(v.activities).map(([name, count]) => `${name} ${count}`).join(" / ") || "관측 없음"}`,
        ];
        for (const line of details) button.append(element("p", line, "muted"));
        button.addEventListener("click", () => {
            byId("village-filter").value = v.id;
            renderNpcs();
            renderVillages(latest.summary);
            byId("npc-panel").scrollIntoView({ behavior: "smooth", block: "start" });
        });
        fragment.append(button);
    }
    byId("villages").replaceChildren(fragment);
}

function matches(npc) {
    const search = byId("search").value.trim().toLocaleLowerCase("ko-KR");
    if (search && !`${npc.name} ${npc.id} ${npc.job}`.toLocaleLowerCase("ko-KR").includes(search)) return false;
    if (byId("village-filter").value && npc.village_id !== byId("village-filter").value) return false;
    if (byId("job-filter").value && String(npc.job_id) !== byId("job-filter").value) return false;
    if (byId("activity-filter").value && String(npc.activity_id) !== byId("activity-filter").value) return false;
    const active = npc.active && npc.enabled;
    switch (byId("state-filter").value) {
        case "active": return active;
        case "hostile": return npc.active && npc.health > 0 && npc.hostile;
        case "loaded": return npc.loaded !== false;
        case "unloaded": return npc.loaded === false;
        case "travelling": return npc.travelling === true;
        case "high": return npc.notoriety >= 60;
        case "down": return npc.health <= 0;
        case "inactive": return !active;
        case "story": return npc.story;
        default: return true;
    }
}

function detailNpc() {
    if (!selectedNpc || !latest) return;
    const npc = latest.snapshot.npcs.find((item) => item.id === selectedNpc);
    put("npc-json", npc ? JSON.stringify(npc, null, 2) : `${selectedNpc}\n현재 로드된 NPC 목록에서 제외되었습니다.`);
}

function renderNpcs() {
    if (!latest) return;
    const npcs = latest.snapshot.npcs.filter(matches).sort((a, b) => Number(b.hostile) - Number(a.hostile) || b.notoriety - a.notoriety || a.id.localeCompare(b.id));
    const fragment = document.createDocumentFragment();
    for (const npc of npcs) {
        const row = element("tr");
        row.classList.toggle("inactive", !(npc.active && npc.enabled));
        const name = element("td");
        const button = element("button", "상세");
        button.type = "button";
        button.setAttribute("aria-label", `${npc.name} 상세`);
        button.addEventListener("click", () => {
            selectedNpc = npc.id;
            detailNpc();
            byId("npc-detail").open = true;
        });
        name.append(element("span", npc.name, "actor-name"), button, element("span", npc.id, "actor-id"));
        row.append(name, element("td", villageNames.get(npc.village_id) || npc.village_id), element("td", npc.job), element("td", npc.health <= 0 ? "제압 / 사망" : npc.activity), element("td", `${npc.health} / ${npc.max_health}`));
        row.append(element("td", `${npc.notoriety} / ${npc.trust}`, npc.notoriety >= 60 ? "danger-text" : ""), element("td", `${npc.crimes} / ${npc.services}`));
        const state = element("td");
        if (npc.loaded === false) state.append(badge("미로드 · 명부", "off"));
        else if (!(npc.active && npc.enabled)) state.append(badge("비활성", "off"));
        else if (npc.health <= 0) state.append(badge("제압 / 사망", "off"));
        else state.append(badge(npc.hostile ? "적대" : "비적대", npc.hostile ? "danger" : ""));
        if (npc.travelling) state.append(element("small", `이동 → ${villageNames.get(npc.destination_town) || npc.destination_town}`));
        if (npc.story) state.append(element("small", "스토리"));
        row.append(state, element("td", coordinates(npc)));
        fragment.append(row);
    }
    if (!npcs.length) {
        const row = element("tr");
        const cell = element("td", "조건에 맞는 NPC가 없습니다.", "empty");
        cell.colSpan = 9;
        row.append(cell);
        fragment.append(row);
    }
    byId("npc-rows").replaceChildren(fragment);
    put("npc-results", `${fmt(npcs.length)} / ${fmt(latest.snapshot.npcs.length)}명 표시`);
    detailNpc();
}

function renderMobs(snapshot) {
    const fragment = document.createDocumentFragment();
    for (const mob of snapshot.mobs) {
        const row = element("tr");
        row.classList.toggle("inactive", !mob.enabled);
        const name = element("td");
        name.append(element("span", mob.name, "actor-name"), element("span", mob.id, "actor-id"));
        row.append(name, element("td", villageNames.get(mob.village_id) || mob.village_id), element("td", `${mob.health} / ${mob.max_health}`), element("td", mob.state), element("td", `${fmt(mob.windup, 2)} / ${fmt(mob.cooldown, 2)}초`), element("td", `${fmt(mob.respawn, 1)}초`), element("td", mob.loot ? "드롭 대기 / 습득 가능" : "없음"), element("td", coordinates(mob)));
        fragment.append(row);
    }
    if (!snapshot.mobs.length) {
        const row = element("tr");
        const cell = element("td", "현재 관측 범위에 몹이 없습니다.", "empty");
        cell.colSpan = 8;
        row.append(cell);
        fragment.append(row);
    }
    byId("mob-rows").replaceChildren(fragment);
}

function renderMessage() {
    if (latest && byId("raw-detail").open) put("raw-json", JSON.stringify(latest, null, 2));
}

function render(data) {
    const connection = byId("connection");
    connection.textContent = data.connected ? "JSON 수신 중" : data.snapshot ? "수신 지연 / 연결 끊김" : "게임 데이터 대기 중";
    connection.className = `badge ${data.connected ? "" : "warn"}`;
    put("received", data.received_at ? `마지막 수신 ${new Date(data.received_at).toLocaleTimeString("ko-KR")} · ${fmt(data.age_seconds, 1)}초 전` : "게임과 수신 서버를 실행하세요.");
    put("record-status", data.record_error ? `JSONL 기록 오류: ${data.record_error}` : data.recording_enabled ? `JSONL 기록 중: ${data.record_path}` : "JSONL 기록이 비활성화되어 있습니다.");
    if (!data.snapshot) return;
    if (latest && latest.snapshot.session_id !== data.snapshot.session_id) selectedNpc = null;
    latest = data;
    const s = data.snapshot;
    const c = data.summary.counts;
    const p = s.profiling;
    const villages = data.summary.villages;
    villageNames = new Map(villages.map((v) => [v.id, v.name]));
    put("simulation", `NPC / 전투 ${s.simulation.paused ? `일시정지 · ${s.simulation.reason}` : "진행 중"}${data.stale ? " · 마지막 관측값" : ""}`);
    put("npc-count", fmt(c.npc_active));
    put("npc-extra", `로드 ${fmt(c.npc_loaded)} · 생존 ${fmt(c.npc_living)} · 비활성 ${fmt(c.npc_loaded - c.npc_active)}`);
    put("mob-count", fmt(c.mob_combat));
    put("mob-extra", `로드 ${fmt(c.mob_loaded)} · 생존 ${fmt(c.mob_living)} · 제압 ${fmt(c.mob_defeated)}`);
    put("hostile-count", fmt(c.npc_hostile));
    put("village-count", fmt(s.world.configured_towns ?? villages.filter((v) => v.kind !== "roads").length));
    put("world-extra", `청크 ${fmt(s.world.loaded_chunks)} · 등록 NPC ${fmt(s.world.registered_npcs ?? s.npcs.length)} · Actor ${fmt(s.world.actors)}`);
    put("fps", p.frame === "0" ? "—" : fmt(p.fps, 1));
    put("draws", fmt(p.draw_total));
    put("draw-extra", `월드 ${p.draw_world} / UI ${p.draw_overlay} / 후처리 ${p.draw_post}`);
    put("player", `레벨 ${s.player.level} · HP ${s.player.health}/${s.player.max_health} · 정당성 ${s.player.legitimacy} · 인지도 ${s.player.reputation} · ${coordinates(s.player)}`);
    put("objective", s.player.objective);
    put("performance", `CPU 갱신 ${fmt(p.update_cpu_ms, 2)} / 렌더 ${fmt(p.render_cpu_ms, 2)} / 제출 ${fmt(p.submit_cpu_ms, 2)} / 후처리 ${fmt(p.post_cpu_ms, 2)} ms · 스왑 ${fmt(p.swap_ms, 2)} ms`);
    put("transport", `인스턴스 ${fmt(p.instances)} · 업로드 ${fmt(p.upload_bytes / 1024, 1)} KiB · 전송 성공 ${s.transport.sent} / 실패 ${s.transport.failed} / 대기 교체 ${s.transport.replaced}`);
    put("message-meta", `세션 ${s.session_id} · 메시지 #${s.sequence} · 월드 초기화 #${s.reset_epoch} · 수집 주기 ${s.interval_ms} ms · 프레임 #${p.frame} · 기억된 NPC 이력 ${s.world.remembered_npcs}`);
    options("village-filter", villages.map((v) => [v.id, v.name]), "모든 마을");
    options("job-filter", [...new Map(s.npcs.map((n) => [n.job_id, n.job])).entries()].sort((a, b) => a[0] - b[0]), "모든 직업");
    options("activity-filter", [...new Map(s.npcs.map((n) => [n.activity_id, n.activity])).entries()].sort((a, b) => a[0] - b[0]), "모든 행동");
    renderVillages(data.summary);
    renderNpcs();
    renderMobs(s);
    renderMessage();
    byId("download").disabled = false;
}

async function poll() {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 3000);
    try {
        const response = await fetch("/api/snapshot", { cache: "no-store", signal: controller.signal });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        render(await response.json());
    } catch (error) {
        put("connection", "수신 서버 연결 끊김");
        byId("connection").className = "badge danger";
        put("received", `서버를 확인하세요. ${error.name === "AbortError" ? "응답 시간 초과" : error.message}`);
        put("simulation", "마지막 관측값 · 현재 게임 상태는 확인할 수 없습니다.");
    } finally {
        clearTimeout(timeout);
        setTimeout(poll, 1000);
    }
}

for (const id of filters) byId(id).addEventListener(id === "search" ? "input" : "change", () => {
    renderNpcs();
    if (latest) renderVillages(latest.summary);
});
byId("raw-detail").addEventListener("toggle", renderMessage);
byId("download").addEventListener("click", () => {
    if (!latest) return;
    const blob = new Blob([JSON.stringify(latest, null, 2)], { type: "application/json;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const link = element("a");
    link.href = url;
    link.download = `world-snapshot-${latest.snapshot.sequence}.json`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
});
poll();
