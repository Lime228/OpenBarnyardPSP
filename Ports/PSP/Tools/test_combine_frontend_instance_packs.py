import json
import struct
import tempfile
import unittest
from pathlib import Path

from combine_frontend_instance_packs import ENTRY, HEADER, MAGIC, VERSION, combine


class CombineFrontendInstancePacksTests(unittest.TestCase):
    def make_source(self, root, name, model_index, instances, texture_payload):
        source = root / name
        source.mkdir()
        prefix = f"frontend_instances_{name}"
        mesh_name = f"{prefix}_model_0.pmes"
        texture_name = f"{prefix}_texture_0.ptex"
        (source / mesh_name).write_bytes(name.encode("ascii"))
        (source / texture_name).write_bytes(texture_payload)
        (source / f"{prefix}.pcfg").write_bytes(
            HEADER.pack(MAGIC, VERSION, 1, 1, ENTRY.size, 0)
            + ENTRY.pack(0, 0, 1, 0, 0)
        )
        manifest = {
            "version": 1,
            "prefix": prefix,
            "model_index": model_index,
            "model_name": name,
            "model_path": f"data/models/{name}.trb",
            "source_meshes": 1,
            "source_instances": instances,
            "models": [
                {
                    "mesh": mesh_name,
                    "material": "fencewhite",
                    "source_texture": "World\\fencewhite.tga",
                    "texture_index": 0,
                    "vertices": 3,
                    "indices": 3,
                    "bytes": len(name),
                }
            ],
            "textures": [{"texture": texture_name, "source_texture": "World\\fencewhite.tga"}],
        }
        path = source / f"{prefix}.json"
        path.write_text(json.dumps(manifest), encoding="utf-8")
        return path

    def test_combines_models_and_deduplicates_exact_texture(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = self.make_source(root, "fncw01", 8, 52, b"shared")
            second = self.make_source(root, "fncw02", 9, 53, b"shared")
            output = root / "combined"
            result = combine([first, second], output, "frontend_instances_fncw")
            self.assertEqual(result["source_instances"], 105)
            self.assertEqual(len(result["models"]), 2)
            self.assertEqual(len(result["textures"]), 1)
            config = (output / "frontend_instances_fncw.pcfg").read_bytes()
            self.assertEqual(HEADER.unpack_from(config), (MAGIC, VERSION, 2, 1, ENTRY.size, 0))
            self.assertEqual(ENTRY.unpack_from(config, HEADER.size), (0, 0, 1, 0, 0))
            self.assertEqual(ENTRY.unpack_from(config, HEADER.size + ENTRY.size), (1, 0, 1, 0, 0))

    def test_rejects_mismatched_duplicate_texture_payload(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = self.make_source(root, "fncw01", 8, 52, b"first")
            second = self.make_source(root, "fncw02", 9, 53, b"second")
            with self.assertRaisesRegex(ValueError, "payload mismatch"):
                combine([first, second], root / "combined", "frontend_instances_fncw")


if __name__ == "__main__":
    unittest.main()
