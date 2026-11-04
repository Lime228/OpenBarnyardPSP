#!/usr/bin/env python3

import copy
import json
import os
import pathlib
import tempfile
import unittest

from convert_barnyard_skin import read_summary, run_pipeline, safe_name, stage_texture
from convert_tmd_dump import build_mesh, build_skeleton


MODEL_DUMP = {
    "version": 1,
    "skeleton": [
        {"name": "root", "parent": None, "translation": [0, 0, 0], "rotation": [0, 0, 0, 1]},
        {"name": "marker", "parent": 0, "translation": [0, 1, 0], "rotation": [0, 0, 0, 1]},
    ],
    "lods": [
        {
            "shader": "skin",
            "meshes": [
                {
                    "material": "probe",
                    "submeshes": [
                        {
                            "bone_palette": [0, 1],
                            "vertices": [
                                {"position": [0, 0, 0], "uv": [0, 0], "bones": [0, 1, 0, 0], "weights": [255, 0, 0, 0]},
                                {"position": [1, 0, 0], "uv": [1, 0], "bones": [0, 1, 0, 0], "weights": [128, 127, 0, 0]},
                                {"position": [0, 1, 0], "uv": [0, 1], "bones": [0, 1, 0, 0], "weights": [0, 255, 0, 0]},
                            ],
                            "triangle_strip": [0, 1, 2],
                        }
                    ],
                }
            ],
        }
    ],
}

ANIMATION_DUMP = {
    "sequence": "idle",
    "sample_rate": 20,
    "loop": True,
    "frames": [
        {
            "root": {"rotation": [0, 0, 0, 1], "translation": [0, 0, 0]},
            "marker": {"rotation": [0, 0, 0, 1], "translation": [0, 1, 0]},
        },
        {
            "root": {"rotation": [0, 0, 0, 1], "translation": [0, 0, 0]},
            "marker": {"rotation": [0, 0, 0, 1], "translation": [0, 1.5, 0]},
        },
    ],
}


class BarnyardSkinPipelineTests(unittest.TestCase):
    def test_sanitizes_output_names(self) -> None:
        self.assertEqual(safe_name("cow idle/front"), "cow_idle_front")
        with self.assertRaises(ValueError):
            safe_name("..")

    def test_rejects_invalid_preconverted_texture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            source = temporary / "invalid.ptex"
            source.write_bytes(b"not a texture")
            with self.assertRaisesRegex(ValueError, "smaller than its header"):
                stage_texture(source, temporary / "output.ptex")

    def test_rejects_degenerate_preview_bounds(self) -> None:
        document = copy.deepcopy(MODEL_DUMP)
        vertices = document["lods"][0]["meshes"][0]["submeshes"][0]["vertices"]
        for vertex in vertices:
            vertex["position"] = [0.0, 0.0, 0.0]
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            mesh = temporary / "probe.pmes"
            skeleton = temporary / "probe.pskl"
            mesh.write_bytes(build_mesh(document, 2))
            skeleton.write_bytes(build_skeleton(document))
            with self.assertRaisesRegex(ValueError, "bounds are degenerate"):
                read_summary(mesh, skeleton, None, None)

    @unittest.skipIf(os.name == "nt", "temporary shebang executable is a POSIX test fixture")
    def test_failed_late_stage_preserves_existing_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            model = temporary / "model.tmd"
            model.touch()
            invalid_texture = temporary / "invalid.ptex"
            invalid_texture.write_bytes(b"not a texture")
            exporter = temporary / "fake_tmdl.py"
            exporter.write_text(
                "#!/usr/bin/env python3\n"
                "import json, pathlib, sys\n"
                f"model = {MODEL_DUMP!r}\n"
                "output = pathlib.Path(sys.argv[sys.argv.index('-o') + 1])\n"
                "output.write_text(json.dumps(model), encoding='utf-8')\n",
                encoding="utf-8",
            )
            exporter.chmod(0o755)

            output = temporary / "output"
            output.mkdir()
            sentinels = {
                output / "probe.pmes": b"old mesh",
                output / "probe.pskl": b"old skeleton",
                output / "probe.ptex": b"old texture",
            }
            for path, payload in sentinels.items():
                path.write_bytes(payload)

            with self.assertRaisesRegex(ValueError, "smaller than its header"):
                run_pipeline(
                    exporter,
                    model,
                    output,
                    "probe",
                    preview_texture=invalid_texture,
                )
            for path, payload in sentinels.items():
                self.assertEqual(path.read_bytes(), payload)

    @unittest.skipIf(os.name == "nt", "temporary shebang executable is a POSIX test fixture")
    def test_runs_model_and_animation_pipeline(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            model = temporary / "model.tmd"
            key_library = temporary / "library.tkl"
            texture = temporary / "preview.ppm"
            model.touch()
            key_library.touch()
            texture.write_text(
                "P3\n4 8\n255\n" + "\n".join(["255 128 0"] * 32) + "\n",
                encoding="ascii",
            )
            exporter = temporary / "fake_tmdl.py"
            exporter.write_text(
                "#!/usr/bin/env python3\n"
                "import json, pathlib, sys\n"
                f"model = {MODEL_DUMP!r}\n"
                f"animation = {ANIMATION_DUMP!r}\n"
                "output = pathlib.Path(sys.argv[sys.argv.index('-o') + 1])\n"
                "output.write_text(json.dumps(model if '-psp-dump' in sys.argv else animation), encoding='utf-8')\n",
                encoding="utf-8",
            )
            exporter.chmod(0o755)

            output = temporary / "output"
            summary = run_pipeline(
                exporter,
                model,
                output,
                "probe",
                key_library,
                "idle/front",
                20,
                keep_intermediate=True,
                runtime_preview=True,
                preview_texture=texture,
                preview_material="PROBE",
            )

            self.assertEqual(summary["vertices"], 3)
            self.assertEqual(summary["indices"], 3)
            self.assertEqual(summary["bones"], 2)
            self.assertEqual(summary["frames"], 2)
            self.assertEqual(summary["sample_rate"], 20)
            self.assertEqual(summary["material"], "probe")
            self.assertEqual(summary["bounds_min"], [0.0, 0.0, 0.0])
            self.assertEqual(summary["bounds_max"], [1.0, 1.0, 0.0])
            self.assertEqual(summary["preview_center"], [0.5, 0.5, 0.0])
            self.assertEqual(summary["preview_scale"], 2.0)
            self.assertEqual(summary["texture_width"], 4)
            self.assertEqual(summary["texture_height"], 8)
            self.assertEqual(summary["texture_bytes"], 152)
            self.assertTrue((output / "barnyard_preview.pmes").is_file())
            self.assertTrue((output / "barnyard_preview.pskl").is_file())
            self.assertTrue((output / "barnyard_preview.panm").is_file())
            self.assertTrue((output / "barnyard_preview.ptex").is_file())
            self.assertTrue((output / "probe_intermediate" / "animation.json").is_file())


if __name__ == "__main__":
    unittest.main()
