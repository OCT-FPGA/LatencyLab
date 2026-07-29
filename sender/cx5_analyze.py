#!/usr/bin/env python3
import struct, re, sys, statistics
f = open(sys.argv[1] if len(sys.argv)>1 else "refl.pcap", "rb")
gh = f.read(24)
magic, = struct.unpack("<I", gh[:4])
nano = (magic == 0xa1b23c4d)
if magic not in (0xa1b2c3d4, 0xa1b23c4d): sys.exit("unknown pcap magic")
arr = {}
while True:
    rh = f.read(16)
    if len(rh) < 16: break
    ts, tf, il, ol = struct.unpack("<IIII", rh)
    data = f.read(il)
    t_ns = ts*1_000_000_000 + (tf if nano else tf*1000)
    m = re.search(rb"([PB])ct-fpga-id:(\d+)", data)
    if not m: continue
    tag = m.group(1)
    d = arr.setdefault(int(m.group(2)), {})
    if tag not in d: d[tag] = t_ns
diffs = [v[b'P']-v[b'B'] for v in arr.values() if b'P' in v and b'B' in v]
print(f"pairs={len(diffs)}")
if diffs:
    diffs.sort(); n=len(diffs)
    print(f"median={statistics.median(diffs):.1f} ns  IQR={diffs[3*n//4]-diffs[n//4]:.1f}  p5={diffs[n//20]:.0f} p95={diffs[19*n//20]:.0f}")
