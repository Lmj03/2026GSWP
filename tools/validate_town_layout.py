"""Validate the shipped world data; never builds or launches the game."""
from dataclasses import dataclass
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Town:
    identity: str
    x: int
    y: int
    width: int
    height: int
    region: int
    security: int
    population: int
    hostiles: int

    @property
    def center(self):
        return (self.x + self.width / 2) * 8, (self.y + self.height / 2) * 8

    def contains(self, x, y):
        return self.x <= x < self.x + self.width and self.y <= y < self.y + self.height


def records(path):
    return [parts for line in path.read_text(encoding="utf-8-sig").splitlines()
            if (parts := line.partition("#")[0].split())]


def validate():
    rows = records(ROOT / "SimpleGame/Data/World/towns.world")
    assert rows[0] == ["version", "1"]
    towns = {}
    links = []
    for row in rows[1:]:
        if row[0] == "town":
            assert len(row) == 10
            town = Town(row[1], *map(int, row[2:]))
            assert town.identity not in towns
            assert town.width == town.height == 7
            assert town.population == 70
            assert 0 <= town.hostiles <= town.population
            towns[town.identity] = town
        else:
            assert row[0] == "road" and len(row) == 3
            links.append(tuple(row[1:]))
    assert len(towns) == 7 and next(iter(towns)) == "afterglow"
    assert sum(t.population for t in towns.values()) == 490
    values = list(towns.values())
    for i, town in enumerate(values):
        for other in values[i + 1:]:
            assert not (town.x < other.x + other.width and town.x + town.width > other.x and
                        town.y < other.y + other.height and town.y + town.height > other.y)
    previous = 71
    for town in sorted(values, key=lambda t: t.security):
        assert town.hostiles < previous, "Higher security must reduce the initial hostile quota"
        previous = town.hostiles
    reached = {"afterglow"}
    unique = set()
    for _ in towns:
        for first, second in links:
            assert first in towns and second in towns and first != second
            if first in reached or second in reached:
                reached.update((first, second))
    assert reached == set(towns)
    travel_times = []
    for first, second in links:
        edge = tuple(sorted((first, second)))
        assert edge not in unique
        unique.add(edge)
        a, b = towns[first], towns[second]
        ax, ay = a.center
        bx, by = b.center
        assert (ax == bx) != (ay == by)
        if ay == by:
            gap = abs(ax - bx) - (a.width + b.width) * 4
        else:
            gap = abs(ay - by) - (a.height + b.height) * 4
        assert gap == 48, (first, second, gap)
        # Existing isometric projection: one axial tile is sqrt(32^2+16^2) pixels.
        seconds = gap * math.hypot(32, 16) / 88
        assert 18 <= seconds <= 22
        travel_times.append(seconds)
    core = records(ROOT / "SimpleGame/Data/afterglow.district")
    for row in core:
        if row[0] == "chunk":
            assert towns["afterglow"].contains(*map(int, row[1:3]))
        elif row[0] == "building":
            x, y, width, depth = map(float, row[2:6])
            for px, py in ((x, y), (x + width - 1e-5, y + depth - 1e-5)):
                assert towns["afterglow"].contains(math.floor(px / 8), math.floor(py / 8))
    for row in records(ROOT / "SimpleGame/Data/Story/act1.story"):
        if row[0] == "node":
            x, y = map(float, row[2:4])
            assert towns["afterglow"].contains(math.floor(x / 8), math.floor(y / 8))
    locale = dict(line.split("=", 1) for line in (ROOT / "SimpleGame/Data/Locale/ko.txt").read_text(encoding="utf-8-sig").splitlines() if line and not line.startswith("#"))
    assert all("town.name." + identity in locale for identity in towns)
    assert "npc.activity.10" in locale and "town.roads" in locale
    print(f"Town data OK: {len(towns)} towns, 49 chunks each, 490 NPC slots, {len(links)} connected roads")
    print(f"Road gap: 48 tiles / theoretical walk {min(travel_times):.2f} seconds; story core inside Afterglow")


if __name__ == "__main__":
    validate()
