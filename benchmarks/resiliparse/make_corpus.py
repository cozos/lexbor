"""Extract HTML response bodies from a WARC into length-prefixed corpora.

usage: python make_corpus.py <in.warc.gz> <out-prefix> [max_docs]

Writes <out-prefix>.raw.bin (HTTP bodies, as the ingest job sees them) and
<out-prefix>.utf8.bin (bytes_to_str(detect_encoding(...)) re-encoded as UTF-8,
which is what resiliparse hands to lexbor). Each record is a little-endian
uint32 length followed by the bytes.
"""
import struct
import sys

from fastwarc.warc import ArchiveIterator, WarcRecordType
from resiliparse.parse.encoding import bytes_to_str, detect_encoding

src, prefix = sys.argv[1], sys.argv[2]
limit = int(sys.argv[3]) if len(sys.argv) > 3 else 0
n = 0
with open(src, 'rb') as f, open(prefix + '.raw.bin', 'wb') as raw, open(prefix + '.utf8.bin', 'wb') as utf8:
    for rec in ArchiveIterator(f, record_types=WarcRecordType.response, parse_http=True,
                               auto_decode='all', max_header_len=1 << 20):
        ct = rec.http_headers.get('Content-Type')
        if ct is None or 'html' not in ct.lower():
            continue
        body = rec.reader.read()
        raw.write(struct.pack('<I', len(body)) + body)
        text = bytes_to_str(body, detect_encoding(body)).encode('utf-8', 'surrogatepass')
        utf8.write(struct.pack('<I', len(text)) + text)
        n += 1
        if limit and n >= limit:
            break
print(n, 'documents')
