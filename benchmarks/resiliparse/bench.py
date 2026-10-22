"""Time resiliparse's extract_plain_text(main_content=True) over a raw corpus.

usage: python bench.py <corpus.raw.bin> [--reps N] [--mode prod|extract]

prod:    extract_plain_text(bytes_to_str(html, detect_encoding(html)), main_content=True)
         (what the WARC ingest does per record)
extract: same, with decoding done before timing starts

Prints per-repetition times and a hash of all outputs: two lexbor builds
must print the same hash.
"""
import argparse
import hashlib
import struct
import time

from resiliparse.extract.html2text import extract_plain_text
from resiliparse.parse.encoding import bytes_to_str, detect_encoding

ap = argparse.ArgumentParser()
ap.add_argument('corpus')
ap.add_argument('--reps', type=int, default=3)
ap.add_argument('--mode', default='prod', choices=['prod', 'extract'])
a = ap.parse_args()

docs = []
with open(a.corpus, 'rb') as f:
    while (b := f.read(4)):
        docs.append(f.read(struct.unpack('<I', b)[0]))
if a.mode == 'extract':
    docs = [bytes_to_str(d, detect_encoding(d)) for d in docs]

times = []
for _ in range(a.reps):
    h = hashlib.sha256()
    t = time.perf_counter()
    for d in docs:
        if a.mode == 'prod':
            d = bytes_to_str(d, detect_encoding(d))
        h.update(extract_plain_text(d, main_content=True).encode('utf-8', 'surrogatepass') + b'\0')
    times.append(time.perf_counter() - t)
print(f'n={len(docs)} times={[round(x, 3) for x in times]} min={min(times):.3f} sha={h.hexdigest()[:16]}')
