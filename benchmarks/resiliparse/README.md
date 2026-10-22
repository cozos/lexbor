# resiliparse workload on lexbor 3.x

resiliparse 1.0.9 ships lexbor 2.4.0 (`phoerious/lexbor@647a0b5`). On the
Common Crawl ingest workload (`extract_plain_text(bytes_to_str(html,
detect_encoding(html)), main_content=True)`, one CC-MAIN-2026-21 WARC, 21,377
HTML records), lexbor 3.1.0 is slower than 2.4.0, and this branch's changes
bring it back below:

| lexbor (resiliparse 1.0.9 built from source against it) | whole WARC | |
|---|---|---|
| 2.4.0 (647a0b5) | 51.1 s | |
| 3.1.0 (master) | 56.6 s | +11% vs 2.4.0 |
| this branch | 47.4 s | -16% vs 3.1.0 |
| this branch, documents created with `LXB_DOM_DOCUMENT_OPT_WO_EVENTS` | 45.9 s | -19% vs 3.1.0 |

All four 3.x rows produce identical output. 3.x differs from 2.4.0 on about
0.05% of documents (2 of 4,000), from parser and selector changes (class
selectors are case-sensitive outside quirks mode in 3.x).

## `<select>` with many options

In 3.x every `<option>` inserted by the parser runs the select's selectedness
algorithm over all of its options (`lxb_html_option_element_insert_steps()` →
`lxb_html_select_selectedness_setting_algorithm()`), so parsing a select with
n options is O(n²). The 8 documents of this WARC with 3,700-31,000 options take
1.62 s on 3.x and 0.22 s on 2.4.0 or with `LXB_DOM_DOCUMENT_OPT_WO_EVENTS`.
Mutation events only drive option selectedness, `<selectedcontent>` and style
here, none of which text extraction uses.

Also in `lxb_html_select_selectedness_setting_algorithm()`: when the display
size is not 1, `snctx.selected` is read without being initialized.

## Building resiliparse 1.0.9 against this branch

`resiliparse-1.0.9.patch` adds the include 3.x needs for `lxb_selectors_*` and
sets `LXB_DOM_DOCUMENT_OPT_WO_EVENTS` on every document. Build lexbor, put its
`include/` and `lib/` under `<dir>/x64-linux/`, and run
`RESILIPARSE_VCPKG_PATH=<dir> pip install .` in the patched sdist.

The node reference counting commit on this branch is the minimum resiliparse
needs to build and to run text extraction. `647a0b5` also makes every
interface destroy function respect the count; port the rest of it before
using resiliparse's DOM editing API (removing nodes still referenced from
Python) with this branch.

The tools here are the same as on `perf/resiliparse-2.4`, whose README has
usage and per-change numbers.
