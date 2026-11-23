import unittest

from convert_retail_model_animations import clip_sample_rate, select_library


class TestRetailModelAnimations(unittest.TestCase):
    def setUp(self) -> None:
        self.model = {"source": "Data/Models/cow.trb", "translations": [2, 3], "quaternions": [1, 2], "scales": [0, 0]}
        self.library = {"source": "Data/Models/shared.tkl", "translations": 5, "quaternions": 3, "scales": 0, "sha256": "a"}

    def test_library_ambiguity_requires_identical_bytes_or_matching_source(self) -> None:
        alias = dict(self.library, source="Data/Models/alias.tkl")
        self.assertEqual(select_library(self.model, [self.library, alias]), ("resolved", [self.library, alias]))
        different = dict(alias, sha256="b")
        self.assertEqual(select_library(self.model, [self.library, different]), ("ambiguous-library", [self.library, different]))
        local = dict(different, source="Data/Models/COW.TKL")
        self.assertEqual(select_library(self.model, [self.library, local]), ("resolved", [local]))

    def test_same_stem_cannot_override_key_range(self) -> None:
        local = dict(self.library, source="Data/Models/cow.tkl", quaternions=2)
        self.assertEqual(select_library(self.model, [local, self.library]), ("resolved", [self.library]))
        self.assertEqual(select_library(self.model, [local]), ("incompatible-library", []))

    def test_sample_budget_accounts_for_clamped_endpoint(self) -> None:
        clip = {"duration": 4.0, "overlay": False, "mode": 0}
        self.assertEqual(clip_sample_rate(clip), 16)
        self.assertEqual(clip_sample_rate(dict(clip, mode=1)), 15)
        self.assertEqual(clip_sample_rate(dict(clip, duration=90.0)), 0)
        self.assertEqual(clip_sample_rate(dict(clip, overlay=True)), 0)


if __name__ == "__main__":
    unittest.main()
