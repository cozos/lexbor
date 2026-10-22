# resiliparse workload: benchmarks and checks

Tools used to profile and verify the lexbor changes on this branch against the
workload of the Common Crawl ingest: `extract_plain_text(bytes_to_str(html,
detect_encoding(html)), main_content=True)` from resiliparse 1.0.9, which ships
lexbor `phoerious/lexbor@647a0b5` (2.4.0 plus node reference counting) as
`resiliparse.libs/liblexbor-a4a9ec82.so.2.4.0`.

This branch is ABI-compatible with that library: build it and replace the file
in an installed resiliparse 1.0.9 wheel. No resiliparse rebuild is needed.

## Files

| File | What it does |
|---|---|
| `make_corpus.py` | WARC → `<prefix>.raw.bin` (HTTP bodies) and `<prefix>.utf8.bin` (decoded as resiliparse does), length-prefixed |
| `bench.py` | times resiliparse over a raw corpus and prints a hash of every output (`--mode prod` includes encoding detection) |
| `harness.c` | the lexbor calls resiliparse makes per document (parse, its two selector queries, the attribute lookups of `_is_main_content_node`, `text_content`), without Python. `verify` mode hashes the serialized DOM |
| `seltest.c` | differential test of `lxb_selectors_find()`: callback order, specificity, `MATCH_FIRST`, `MATCH_ROOT` and `STOP` over many simple selector lists |
| `build_pgo.sh` | `-fno-semantic-interposition`, LTO and PGO build trained on the workload |

## Usage

```sh
pip install resiliparse==1.0.9 fastwarc==1.0.9
python make_corpus.py CC-MAIN-...-00000.warc.gz /data/w0          # ~21k documents per WARC

cmake -S ../.. -B /tmp/lexbor-build -DLEXBOR_BUILD_SHARED=ON -DLEXBOR_BUILD_STATIC=OFF \
      -DLEXBOR_OPTIMIZATION_LEVEL=-O3 -DCMAKE_C_FLAGS=-fno-semantic-interposition \
      -DCMAKE_INSTALL_PREFIX=/tmp/lexbor
cmake --build /tmp/lexbor-build -j && cmake --install /tmp/lexbor-build

LIBS=$(python -c 'import resiliparse, os; print(os.path.dirname(resiliparse.__file__) + ".libs")')
cp $LIBS/liblexbor-a4a9ec82.so.2.4.0 /tmp/liblexbor.shipped.so
python bench.py /data/w0.raw.bin                                    # before
cp /tmp/lexbor/lib/liblexbor.so.2.4.0 $LIBS/liblexbor-a4a9ec82.so.2.4.0
python bench.py /data/w0.raw.bin                                    # after: same sha

cc -O2 harness.c -I/tmp/lexbor/include -L/tmp/lexbor/lib -llexbor -Wl,-rpath,/tmp/lexbor/lib -o harness
./harness /data/w0.utf8.bin 1 verify                                # DOM + query hash
cc -O2 seltest.c -I/tmp/lexbor/include -L/tmp/lexbor/lib -llexbor -Wl,-rpath,/tmp/lexbor/lib -o seltest
./seltest /data/w0.utf8.bin                                         # compare with a 647a0b5 build
```

## Results (CC-MAIN-2026-21 segment 00000 WARC, 21,377 HTML records, one core)

All outputs are byte-identical to the shipped library: the hashes of all 21,377
`extract_plain_text` results match, and so do the serialized DOMs (`harness
verify`), the selector differential test (6.49M callbacks) and lexbor's 119 unit
tests.

| Library in the resiliparse 1.0.9 wheel | whole WARC, `--mode prod` | vs shipped |
|---|---|---|
| shipped `liblexbor-a4a9ec82.so.2.4.0` (GCC 14) | 49.1 s | |
| 647a0b5 rebuilt (GCC 13, -O3) | 50.2 s | +2% |
| this branch, -O3 | 43.8 s | -10.8% |
| this branch, -O3 -fno-semantic-interposition | 42.9 s | -12.6% |
| this branch, `build_pgo.sh` (-fno-semantic-interposition, LTO, PGO) | 41.0 s | **-16.5%** |
| same, with `LD_PRELOAD=libmimalloc.so.2` | 40.4 s | -17.7% |

Lexbor's own instructions in this workload went down 37%. Per change, as
instructions of `extract_plain_text` on 300 documents (callgrind, 2,548M at
647a0b5):

| Change | Δ instructions |
|---|---|
| `lxb_dom_element_attr_by_name()`: no hashing of the requested name; early return without attributes | -5.9% |
| `lxb_selectors_find()`: direct loop for lists of simple selectors | -8.3% |
| class attribute tokenized once for lists of class selectors | -3.1% |
| SSE2 seeks in data/RCDATA/RAWTEXT/script/attribute-value states, re-seek after each special character | -5.2% |
| type selectors resolved once with a tag bitmap; failed name lookups cached | -3.2% |
| static hash lookup: 32-bit modulo, inline compare | -0.5% |
| `-fno-semantic-interposition` / LTO / PGO (build only) | -2.0% / -3.4% / -2.6% |

Single runs on a 4-vCPU Xeon VM with warm page cache; interleaved runs on a
1,000-document subset vary by about ±3%.
