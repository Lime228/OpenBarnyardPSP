import struct
import tempfile
import unittest
from pathlib import Path

from inventory_retail_sequence import describe_resources, inspect_script, retail_file
from package_retail_sequence_opening import camera_shots, opening_shots


class RetailSequenceTests(unittest.TestCase):
    def test_opening_keeps_the_master_wait_durations_and_camera_changes(self):
        events = [
            {"type": "command", "name": command, "arguments": [value]}
            for command, value in (("bindCamera", "camera"), ("SetTerrain", "scene"), ("Set_FOV", "50"), ("SetNearClip", "0.8"))
        ]
        events += [
            {"type": "animation", "name": "first"},
            {"type": "command", "name": "SetNearClip", "arguments": ["2"]},
            {"type": "animation", "name": "second"},
            {"type": "command", "name": "SetNearClip", "arguments": ["0.5"]},
            {"type": "animation", "name": "third"},
        ]
        timing = [
            {"type": "wait", "flag": 1, "duration": 0}, {"type": "wait", "flag": 2, "duration": 0},
            {"type": "resume"}, {"type": "wait", "flag": 0, "duration": 4.4}, {"type": "resume"},
            {"type": "wait", "flag": 0, "duration": 4}, {"type": "resume"},
            {"type": "wait", "flag": 0, "duration": 4.633334}, {"type": "resume"},
        ]
        voice = {"actor": 15, "type": "vostream", "opens_track": True, "index": 1}
        script = {"camera_tracks": [{"actor": 14, "events": events}], "master_timing": timing, "voice_instructions": [voice]}
        actor, shots, selected_voice = opening_shots(script, 3)
        self.assertEqual(actor, 14)
        self.assertEqual(selected_voice, voice)
        self.assertEqual([(shot["clip"], shot["duration"], shot["near_clip"]) for shot in shots],
                         [("first", 4.4, 0.8), ("second", 4.0, 2.0), ("third", 4.633334, 0.5)])
        timing[3]["flag"] = 9
        with self.assertRaisesRegex(ValueError, "opening synchronization"):
            opening_shots(script, 3)

    def test_camera_track_preserves_the_player_creation_gate_and_terrain_change(self):
        events = [
            {"type": "command", "name": name, "arguments": [value]}
            for name, value in (("bindCamera", "camera"), ("SetTerrain", "Barn"), ("Set_FOV", "50"), ("SetNearClip", "0.8"))
        ]
        events += [
            {"type": "animation", "name": "before"},
            {"type": "animation", "name": "SS01_GUI_idle"},
            {"type": "command", "name": "SetTerrain", "arguments": ["BeadyIntDay"]},
            {"type": "animation", "name": "after"},
        ]
        resume = {"type": "resume"}
        master = [
            {"type": "wait", "flag": 1}, {"type": "wait", "flag": 2}, resume,
            {"type": "wait", "flag": 0, "duration": 11}, {"type": "wait", "flag": 3}, resume,
            resume, {"type": "wait", "flag": 0, "duration": 8.23}, resume,
        ]
        cues = [{"type": "command", "name": "UI", "arguments": ["Player_select"], "after_resume": 2}]
        _, shots = camera_shots({"camera_tracks": [{"actor": 14, "events": events}],
                                 "master_timing": master, "master_cues": cues})
        self.assertEqual([(shot["duration"], shot["wait_flags"], shot["ui"], shot["visibility_row"]) for shot in shots],
                         [(11, [3], None, "Barn"), (None, [], "Player_select", "Barn"),
                          (8.23, [], None, "BeadyIntDay")])

    def test_inspects_the_original_tso_tables(self):
        header = struct.pack("<6I", 56, 8, 64, 76, 8, 84)
        script = struct.pack("<IiIII", 0, 0, 1, 0, 4)
        voice = struct.pack("<iIiIii", 5, 0, 0, 6, 2, 0)
        end = struct.pack("<iIi", 13, 0, 0)
        tables = struct.pack("<II12sIIiI", 0, 32, b"Intro\0Line", 12, 16, 7, 20)
        body = voice + end + tables
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Intro.tso"
            path.write_bytes(header + script + body)
            self.assertEqual(inspect_script(path), {
                "name": "Intro", "tracks": 1, "track_actors": [7],
                "voice_instructions": [{"track": 0, "actor": 7, "type": "vostream", "index": 2, "name": "Line", "opens_track": True}],
                "camera_tracks": [],
                "master_timing": [],
                "master_cues": [],
                "attachment_tracks": [],
                "animation_tracks": [],
                "visibility_tracks": [],
            })

            path.write_bytes(header + script + body[:-1])
            with self.assertRaisesRegex(ValueError, "track tables"):
                inspect_script(path)

            path.write_bytes(header + script + voice + struct.pack("<iIi", 10, 0, 0) + tables)
            with self.assertRaisesRegex(ValueError, "missing TSO track end"):
                inspect_script(path)

    def test_resolves_camera_commands_and_clip_through_string_relocations(self):
        strings = b"Intro\0bindCamera\0CH_Cam\0CH_CamT\0FirstShot\0Hide\0show\0"
        strings += bytes(-len(strings) % 4)
        script = struct.pack("<IiIII", 0, 0, 1, 0, 4)
        command = struct.pack("<iIi4I", 12, 0, 0, strings.index(b"bindCamera"),
                              strings.index(b"CH_Cam"), strings.index(b"CH_CamT"), 0)
        clip = struct.pack("<iIi4I", 4, 4, 0, 0xFFFFFFFF, strings.index(b"FirstShot"), 1, 0)
        hide = struct.pack("<iIi4I", 12, 0, 0, strings.index(b"Hide"), 0, 0, 0)
        pause = struct.pack("<iIi", 10, 0, 0)
        show = struct.pack("<iIi4I", 12, 0, 0, strings.index(b"show"), 0, 0, 0)
        end = struct.pack("<iIi", 13, 0, 0)
        code = script + command + clip + hide + pause + show + end
        string_relocs = struct.pack("<7I", 0, 32, 36, 40, 64, 88, 128)
        pointer_relocs = struct.pack("<2I", 12, 16)
        pointer_base = len(code) + len(string_relocs) + len(strings) + len(pointer_relocs)
        header = struct.pack("<6I", len(code), len(string_relocs), len(code) + len(string_relocs),
                             pointer_base - len(pointer_relocs), len(pointer_relocs), pointer_base)
        pointers = struct.pack("<iI", 14, len(script))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Intro.tso"
            path.write_bytes(header + code + string_relocs + strings + pointer_relocs + pointers)
            inventory = inspect_script(path)
            self.assertEqual(inventory["camera_tracks"], [{
                "track": 0, "actor": 14, "events": [
                    {"type": "command", "name": "bindCamera", "arguments": ["CH_Cam", "CH_CamT", None]},
                    {"type": "animation", "name": "FirstShot"},
                ],
            }])
            self.assertEqual(inventory["animation_tracks"], [{
                "track": 0, "actor": 14, "clips": [{"clip": "FirstShot", "after_pause": 0}],
            }])
            self.assertEqual(inventory["visibility_tracks"], [{
                "track": 0, "actor": 14, "events": [
                    {"name": "Hide", "after_pause": 0, "after_clip": "FirstShot"},
                    {"name": "show", "after_pause": 1, "after_clip": None},
                ],
            }])

    def test_uses_model_types_and_the_original_symbol_case(self):
        properties = [["keylib", ["shared"]], ["matlib", ["walls", "other"]]]
        model_types = [
            ["chief", {"properties": [["AnimatableObjectType", {"properties": [["Model", "Leader"]]}]]}],
            ["crowd", {"properties": []}],
        ]
        symbols = {"symbols": [{"name": name} for name in ("Shared_keylib", "Walls_TTL", "Leader_Header", "Leader_FileHeader", "Crowd_Header")]}
        result = describe_resources(properties, model_types, symbols)
        self.assertEqual(result["key_libraries"], [{"name": "shared", "symbol": "Shared_keylib"}])
        self.assertEqual(result["material_libraries"], [{"name": "walls", "symbol": "Walls_TTL"}, {"name": "other", "symbol": None}])
        self.assertEqual([model["model"] for model in result["model_types"]], ["Leader", "crowd"])
        self.assertEqual([model["packed_header"] for model in result["model_types"]], ["Leader_Header", "Crowd_Header"])

    def test_resolves_retail_path_separators_and_case(self):
        with tempfile.TemporaryDirectory() as directory:
            install = Path(directory)
            path = install / "Data" / "Script.TRB"
            path.parent.mkdir()
            path.touch()
            self.assertEqual(retail_file(install, "data\\script.trb"), path)
            with self.assertRaisesRegex(ValueError, "invalid retail file reference"):
                retail_file(install, "Data/../../Script.TRB")
