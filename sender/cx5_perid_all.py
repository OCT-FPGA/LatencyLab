#!/usr/bin/env python3
import struct, re, sys
f = open(sys.argv[1] if len(sys.argv)>1 else "refl_p4_C.pcap", "rb")
gh = f.read(24)
magic, = struct.unpack("<I", gh[:4])
nano = (magic == 0xa1b23c4d)
if magic not in (0xa1b2c3d4, 0xa1b23c4d): sys.exit("unknown pcap magic")
recs=[]
while True:
    rh = f.read(16)
    if len(rh) < 16: break
    ts, tf, il, ol = struct.unpack("<IIII", rh)
    data = f.read(il)
    t_ns = ts*1_000_000_000 + (tf if nano else tf*1000)
    m = re.search(rb"([PB])ct-fpga-id:(\d+)", data)
    if not m: continue
    recs.append((t_ns, m.group(1), int(m.group(2))))
recs.sort()
out=open("silicon_perid_all.csv","w")
out.write("run,pkt_id,silicon_diff_ns\n")
run=1; last=None; cur={}
def flush(run,cur,out):
    n=0
    for i,v in cur.items():
        if b'P' in v and b'B' in v:
            out.write(f"{run},{i},{v[b'P']-v[b'B']}\n"); n+=1
    return n
tot=0
for t_ns, tag, i in recs:
    if last is not None and t_ns-last > 1_000_000_000:   # >1s gap = new run
        tot+=flush(run,cur,out); run+=1; cur={}
    last=t_ns
    d=cur.setdefault(i,{})
    if tag not in d: d[tag]=t_ns
tot+=flush(run,cur,out)
out.close()
print(f"runs={run} total_pairs={tot} -> silicon_perid_all.csv")
