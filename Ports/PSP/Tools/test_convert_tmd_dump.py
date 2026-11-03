#!/usr/bin/env python3

import unittest

from convert_tmd_dump import reduce_weights, triangulate_strip


class TMDPSPBridgeTests(unittest.TestCase):
    def test_reduces_four_weights_to_two_strongest_global_bones(self) -> None:
        self.assertEqual(
            reduce_weights([0, 1, 2], [0, 1, 2, 0], [5, 20, 220, 10], 3),
            (2, 1, 234, 21),
        )

    def test_combines_duplicate_bones_before_reduction(self) -> None:
        self.assertEqual(
            reduce_weights([2, 1], [0, 0, 1, 1], [100, 50, 60, 40], 3),
            (2, 1, 153, 102),
        )

    def test_converts_strip_winding_and_skips_degenerates(self) -> None:
        self.assertEqual(triangulate_strip([0, 1, 2, 3], 4), [0, 1, 2, 2, 1, 3])
        self.assertEqual(triangulate_strip([0, 1, 1, 2, 3], 4), [1, 2, 3])


if __name__ == "__main__":
    unittest.main()
