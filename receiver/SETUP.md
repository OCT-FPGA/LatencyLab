# From-scratch setup: P4-on-FPGA latency measurement (pc163 / receiver node)

Everything needed to rebuild the DPDK measurement environment on the receiver
node after an experiment restart. Assumes a fresh OCT node matching pc163:
AU280 FPGA at PCIe 3b:00.0 / 3b:00.1, onboard ConnectX-5 at PCIe 86:00.0
(interface enp134s0np0), bitstreams + pcaps in ~/au280_<name>/.

## 0. One-time host prep

    # kernel cmdline must contain (check first, edit GRUB + reboot if missing):
    cat /proc/cmdline
    #   default_hugepagesz=1G hugepagesz=1G hugepages=4 intel_iommu=on

    sudo apt install --assume-yes build-essential libnuma-dev pkg-config \
      python3-pip python3-setuptools python3-wheel python3-pyelftools \
      ninja-build libpcap-dev tcpreplay linuxptp
    sudo pip3 install meson scapy

pcimem: copy `opennic-scripts` from your OCT deployment share to `~/`, then
`cd ~/opennic-scripts/pcimem && make`.

## 1. DPDK stack (once per node)

    cd ~
    git clone https://github.com/Xilinx/dma_ip_drivers.git
    cd dma_ip_drivers && git checkout 7859957 && cd ..
    git clone https://github.com/Xilinx/open-nic-dpdk.git
    cp open-nic-dpdk/*.patch dma_ip_drivers/
    cd dma_ip_drivers && git apply *.patch && cd ..

    wget https://fast.dpdk.org/rel/dpdk-20.11.tar.xz && tar xf dpdk-20.11.tar.xz
    cd dpdk-20.11
    cp -R ../dma_ip_drivers/QDMA/DPDK/drivers/net/qdma ./drivers/net
    sed -i "47i 'qdma'," drivers/net/meson.build
    meson build && cd build && ninja && sudo ninja install && sudo ldconfig

    # devbind patch:
    cd ../usertools
    patch dpdk-devbind.py < ~/open-nic-dpdk/dpdk-devbind.diff

## 2. Measurement app (once per node)

This repo's receiver code already includes the final version
(`../p4lat_r2.c` + `../Makefile`) — just build it:

    cd ~/LatencyLab/receiver
    make

## 3. Per-bitstream: flash + bring-up ("the dance")

Flash via `Scripts/flash.py <name>` (config-fpga reset -> xbflash2 program ->
config-fpga boot -> onic driver reload), then:

    # 1. clean PCIe (FPGA ports)
    sudo ~/dpdk-20.11/usertools/dpdk-devbind.py -u 3b:00.0 3b:00.1 2>/dev/null
    echo 1 | sudo tee /sys/bus/pci/devices/0000:3b:00.0/remove
    echo 1 | sudo tee /sys/bus/pci/devices/0000:3b:00.1/remove
    sleep 2; echo 1 | sudo tee /sys/bus/pci/rescan; sleep 3
    lspci -d 10ee:                      # expect 903f + 913f

    # 2. onic driver = the hardware initializer (build once per boot)
    cd ~/opennic-scripts/open-nic-driver && sudo make -s
    sudo rmmod onic 2>/dev/null; sudo insmod onic.ko RS_FEC_ENABLED=0
    ip link show | grep enp135          # both PFs must appear

    # 3. P4 bitstreams only: load tables
    cd ~/au280_<name>/drivers/install && make -s && sudo ./driver

    # 4. CRITICAL: disable the PCI reset (vfio's bus reset kills the shell;
    #    resets to default after every rescan/reboot!)
    echo "" | sudo tee /sys/bus/pci/devices/0000:3b:00.0/reset_method

    # 5. hand the FPGA ports AND the ConnectX-5 to DPDK
    sudo modprobe vfio-pci
    sudo ~/dpdk-20.11/usertools/dpdk-devbind.py -b vfio-pci 3b:00.0 3b:00.1 86:00.0

## 4. Per-session: measure

In the final 2-node campaign this is driven by `campaign163.sh` (this node)
together with `campaign162.sh` (sender node) — see
[`../receiver/README.md`](../receiver/README.md) and
[`../sender/README.md`](../sender/README.md). Manually, the receiver side is:

    cd ~/LatencyLab/receiver && mkdir -p <OUTDIR>
    sudo ./p4lat_r2 -l 2,4,6 -n 4 -a 3b:00.0 -a 3b:00.1 -a 86:00.0 \
      -d librte_net_qdma.so \
      -- --prefix <OUTDIR>/<tag> --runs <N> --dst-mac <sender ConnectX-5 MAC>

    # NOTE: the exact driver flag(s) needed for the ConnectX-5 port (86:00.0)
    # are not confirmed here — verify against actual shell history / re-test
    # before relying on this exact invocation.

fmap regs are cleared whenever a DPDK app starts -> rewrite once AFTER
p4lat_r2 is up, BEFORE the sender starts replaying:

    P=/sys/devices/pci0000:3a/0000:3a:00.0/0000:3b:00.0/resource2
    cd ~/opennic-scripts/pcimem
    sudo ./pcimem $P 0x1000 w 0x1
    sudo ./pcimem $P 0x2000 w 0x00010001

## 5. Standard experiment matrix (per P4 program)

1. Flash program bitstream -> dance -> tables -> 10-run session, its own pcap.
2. Flash plain-onic (null) bitstream -> dance (skip tables) -> 10-run session,
   SAME pcap. (Null offset is traffic-dependent: 33-48 ns across our pcaps.)
3. Lp4 = median(P4 session run medians) - median(null session run medians).
4. Analysis conventions: drop nothing (no warmup in DPDK era), diff column is
   SIGNED rx1(P4) - rx2(bypass), per-run median, mean +/- sd across runs.

## Hard-won rules (violate at your peril)

- Full dance before EVERY DPDK app start. No dance -> dead BAR (0xFFFFFFFF)
  -> PCIe remove/rescan recovers it.
- reset_method must be emptied after every rescan/reboot, BEFORE binding vfio.
- fmap (0x1000/0x2000) rewrite after every app start.
- RX ring size must stay 512.
- Multi-run inside ONE app process: fine. New app process: full dance first.
- FPGA H2C TX does not reach the wire on this platform (ingress-only config);
  reflection uses this node's own ConnectX-5 (86:00.0 / enp134s0np0) for the
  return path, not the FPGA's own ports.
- CX5 hardware timestamping of all RX (on the sender node): hwstamp_ctl -i
  enp134s0np0 -r 1; capture with tcpdump -j adapter_unsynced
  --time-stamp-precision=nano.
