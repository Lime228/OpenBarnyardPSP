#!/usr/bin/env python3

import unittest

from mark_retail_layout_asserts import mark_source


class MarkRetailLayoutAssertsTests(unittest.TestCase):
    def test_marks_only_sizeof_asserts(self):
        source = (
            "TSTATICASSERT( sizeof( Widget ) == 4 );\n"
            "TSTATICASSERT( Count > 0 );\n"
            "TSTATICASSERT_SIZE( sizeof( Marked ) == 8 );\n"
        )

        marked, count = mark_source(source)

        self.assertEqual(count, 1)
        self.assertIn("TSTATICASSERT_SIZE( sizeof( Widget ) == 4 );", marked)
        self.assertIn("TSTATICASSERT( Count > 0 );", marked)
        self.assertEqual(marked.count("TSTATICASSERT_SIZE"), 2)


if __name__ == "__main__":
    unittest.main()
