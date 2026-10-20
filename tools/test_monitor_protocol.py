"""Protocol/aggregation checks only: no game, browser or listening server is launched."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from monitor_server import MAX_BODY, MonitorState, reject_constant, summarize, unique_object, validate_snapshot


def sample():
    return json.loads((Path(__file__).parent / "monitor/sample.snapshot.json").read_text(encoding="utf-8"))


class ProtocolTests(unittest.TestCase):
    def test_counts_do_not_double_count_hostile_npcs_or_scripted_mobs(self):
        data = validate_snapshot(sample())
        result = summarize(data)
        self.assertEqual(result["counts"], {
            "npc_loaded": 4, "npc_active": 3, "npc_living": 2, "npc_hostile": 1,
            "npc_incapacitated": 1, "mob_loaded": 2, "mob_living": 2,
            "mob_combat": 1, "mob_defeated": 0,
        })
        district, outer = result["villages"]
        self.assertEqual(district["avg_notoriety"], 36)
        self.assertEqual(district["jobs"], {"상인": 1, "약탈자": 1})
        self.assertEqual(district["status"], "위험")
        self.assertFalse(district["partial"])
        self.assertTrue(outer["partial"])
        self.assertIsNone(outer["avg_trust"])
        self.assertEqual(outer["status"], "인구 미관측")

    def test_village_danger_unrest_stability_and_defeated_counts(self):
        data = sample()
        for npc in data["npcs"]:
            npc["hostile"] = False
            npc["activity_id"] = 0
        for mob in data["mobs"]:
            mob["combat_enabled"] = False
        self.assertEqual(summarize(data)["villages"][0]["status"], "불안")
        data["npcs"][1]["notoriety"] = 10
        self.assertEqual(summarize(data)["villages"][0]["status"], "안정")
        data["mobs"][0]["health"] = 0
        result = summarize(validate_snapshot(data))
        self.assertEqual(result["counts"]["mob_defeated"], 1)
        self.assertEqual(result["counts"]["mob_living"], 1)

    def test_rejects_bad_types_ranges_ids_and_totals(self):
        mutations = [
            lambda d: d.update(schema_version=True),
            lambda d: d.update(sequence=0),
            lambda d: d.update(npcs="not an array"),
            lambda d: d["npcs"][0].update(health=-1),
            lambda d: d["npcs"][0].update(hostile=1),
            lambda d: d["npcs"][0].update(notoriety=101),
            lambda d: d["npcs"][0].update(activity_id=11),
            lambda d: d["npcs"][0].update(village_id="unknown"),
            lambda d: d["npcs"][1].update(id=d["npcs"][0]["id"]),
            lambda d: d["npcs"][0]["position"].update(chunk_x=9007199254740993),
            lambda d: d["npcs"][0]["position"].update(chunk_x="9223372036854775808"),
            lambda d: d["npcs"][0]["position"].update(local_x=float("nan")),
            lambda d: d["world"].update(loaded_chunks=11),
            lambda d: d["profiling"].update(draw_total=99),
            lambda d: d["npcs"][0].update(name="\ud800"),
            lambda d: d.update(extension=float("inf")),
        ]
        for mutation in mutations:
            data = sample()
            mutation(data)
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                validate_snapshot(data)

    def test_json_rejects_duplicates_nonstandard_constants_and_accepts_korean(self):
        for raw in ('{"id":1,"id":2}', '{"health":NaN}', '{"health":Infinity}'):
            with self.assertRaises(ValueError):
                json.loads(raw, object_pairs_hook=unique_object, parse_constant=reject_constant)
        data = sample()
        data["npcs"][0]["name"] = '한글 "상인"\\길\n다음 줄'
        data["npcs"][0]["position"]["chunk_x"] = "-9007199254740993"
        decoded = json.loads(json.dumps(data, ensure_ascii=False))
        self.assertEqual(validate_snapshot(decoded)["npcs"][0]["name"], data["npcs"][0]["name"])

    def test_sequence_session_restart_reset_stale_and_recording(self):
        with tempfile.TemporaryDirectory() as directory:
            state = MonitorState(Path(directory))
            data = sample()
            try:
                self.assertFalse(state.read()["connected"])
                self.assertTrue(state.accept(data))
                self.assertFalse(state.accept(copy.deepcopy(data)))
                older = copy.deepcopy(data)
                older["sequence"] = 0
                self.assertFalse(state.accept(older))
                with patch("monitor_server.time.monotonic", return_value=state.received + 6):
                    self.assertTrue(state.read()["stale"])
                reset = copy.deepcopy(data)
                reset.update(sequence=2, reset_epoch=2, uptime_seconds=11)
                self.assertTrue(state.accept(reset))
                restarted = copy.deepcopy(data)
                restarted["session_id"] = "new-session"
                self.assertTrue(state.accept(restarted))
                self.assertTrue(state.read()["connected"])
                rows = [json.loads(line) for line in next(Path(directory).glob("*.jsonl")).read_text(encoding="utf-8").splitlines()]
                self.assertEqual(len(rows), 3)
                self.assertEqual(rows[-1]["snapshot"]["session_id"], "new-session")
                self.assertEqual(rows[0]["summary"]["counts"]["npc_hostile"], 1)
            finally:
                state.close()

    def test_offscreen_traveller_is_registered_but_not_counted_as_loaded(self):
        data = sample()
        npc = data["npcs"][1]
        npc.update(loaded=False, enabled=False, travelling=True, activity_id=10,
                   activity="마을 간 이동", home_town="afterglow", destination_town="sector/0/0")
        result = summarize(validate_snapshot(data))
        self.assertEqual(len(data["npcs"]), 4)
        self.assertEqual(result["counts"]["npc_loaded"], 3)
        self.assertEqual(result["counts"]["npc_hostile"], 0)
        npc["destination_town"] = "missing"
        with self.assertRaises(ValueError):
            validate_snapshot(data)

    def test_seven_town_census_and_490_residents_fit_transport(self):
        data = sample()
        rows = [line.split() for line in (Path(__file__).parents[1] / "SimpleGame/Data/World/towns.world").read_text(encoding="utf-8").splitlines() if line.startswith("town ")]
        data["villages"] = []
        prototype = copy.deepcopy(data["npcs"][0])
        data["npcs"] = []
        for index, row in enumerate(rows):
            _, identity, x, y, width, height, region, security, population, hostiles = row
            population, hostiles = int(population), int(hostiles)
            data["villages"].append({"id": identity, "name": identity, "kind": "town",
                                     "loaded_chunks": 49 if index == 0 else 0, "expected_chunks": 49,
                                     "security": int(security), "target_population": population,
                                     "initial_hostiles": hostiles, "population": population,
                                     "living_population": population, "hostile_population": hostiles,
                                     "travelling_population": 0})
            for resident in range(population):
                npc = copy.deepcopy(prototype)
                npc.update(id=f"citizen/town/{identity}/{resident}", loaded=index == 0,
                           enabled=index == 0, hostile=resident < hostiles, home_town=identity,
                           village_id=identity, travelling=False, destination_town="")
                npc["position"].update(chunk_x=str(int(x) + int(width) // 2), chunk_y=str(int(y) + int(height) // 2))
                data["npcs"].append(npc)
        data["world"].update(loaded_chunks=49, registered_npcs=490, configured_towns=7)
        data["player"]["village_id"] = "afterglow"
        result = summarize(validate_snapshot(data))
        self.assertEqual(len(data["npcs"]), 490)
        self.assertEqual(result["counts"]["npc_loaded"], 70)
        self.assertEqual(result["counts"]["npc_hostile"], 12)
        self.assertTrue(all(village["status"] == "위험" for village in result["villages"]))
        self.assertLess(len(json.dumps(data, ensure_ascii=False).encode("utf-8")), MAX_BODY)

    def test_recording_failure_does_not_stop_monitoring(self):
        with tempfile.TemporaryDirectory() as directory:
            state = MonitorState(Path(directory))
            data = sample()
            try:
                with patch("monitor_server.Path.open", side_effect=OSError("disk failure")):
                    self.assertTrue(state.accept(data))
                self.assertTrue(state.read()["connected"])
                self.assertIn("disk failure", state.read()["record_error"])
            finally:
                state.close()


if __name__ == "__main__":
    unittest.main()
