/* p4lat_r2: kernel-free reflector. Ports 0,1 = FPGA PFs (vfio/QDMA).
 * Port 2 = ConnectX-5 (mlx5). Each poller TSC-stamps, tags the marker
 * ('P' = P4 path, 'B' = bypass), rewrites dst MAC, and transmits the SAME
 * mbuf on its own TX queue of port 2. No syscalls anywhere in the loop. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <netinet/ether.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>
#include <rte_memcpy.h>

#define FPGA_PORTS    2
#define REFL_PORT     2
#define RX_RING_SIZE  512
#define TX_RING_SIZE  512
#define NUM_MBUFS     16383
#define MBUF_CACHE    250
#define BURST         32
#define MAX_ID        (1u << 21)

static const char MARKER[] = "oct-fpga-id:";
#define MARKER_LEN (sizeof(MARKER) - 1)

static volatile int quit;
static uint64_t *seen[FPGA_PORTS];
static volatile uint64_t marker_count[FPGA_PORTS];
static volatile uint64_t reflected[FPGA_PORTS];
static uint8_t dst_mac[6];
static struct rte_mempool *mp_refl;

struct worker_arg { uint16_t port; };

static void handle_sig(int sig) { (void)sig; quit = 1; }

static inline uint32_t parse_id(const uint8_t *data, uint32_t len)
{
    if (len < MARKER_LEN + 1) return UINT32_MAX;
    const uint8_t *p = memmem(data, len, MARKER, MARKER_LEN);
    if (!p) return UINT32_MAX;
    p += MARKER_LEN;
    const uint8_t *end = data + len;
    uint64_t id = 0; int nd = 0;
    while (p < end && *p >= '0' && *p <= '9' && nd < 9) { id = id*10 + (*p-'0'); p++; nd++; }
    if (nd == 0 || id >= MAX_ID) return UINT32_MAX;
    return (uint32_t)id;
}

static int rx_worker(void *argp)
{
    struct worker_arg *arg = argp;
    const uint16_t port = arg->port;
    uint64_t *tab = seen[port];
    struct rte_mbuf *bufs[BURST];

    printf("poller port %u on lcore %u (reflect via port %d txq %u, tag '%c')\n",
           port, rte_lcore_id(), REFL_PORT, port, port == 0 ? 'P' : 'B');

    while (!quit) {
        const uint16_t n = rte_eth_rx_burst(port, 0, bufs, BURST);
        if (n == 0) continue;
        for (uint16_t i = 0; i < n; i++) {
            const uint64_t ts = rte_rdtsc_precise();
            uint8_t *data = rte_pktmbuf_mtod(bufs[i], uint8_t *);
            uint32_t len = rte_pktmbuf_data_len(bufs[i]);
            uint32_t id = parse_id(data, len);
            if (id != UINT32_MAX) {
                marker_count[port]++;
                if (tab[id] == 0) tab[id] = ts;
                uint8_t *mk = memmem(data, len, MARKER, MARKER_LEN);
                if (mk && len >= 14) {
                    memcpy(data, dst_mac, 6);
                    mk[0] = (port == 0) ? 'P' : 'B';
                    if (rte_eth_tx_burst(REFL_PORT, port, &bufs[i], 1) == 1) {
                        reflected[port]++;
                        continue;   /* mbuf owned by TX */
                    }
                }
            }
            rte_pktmbuf_free(bufs[i]);
        }
    }
    return 0;
}

static void port_init(uint16_t port, struct rte_mempool *mp, uint16_t ntxq)
{
    struct rte_eth_conf conf; memset(&conf, 0, sizeof(conf));
    if (rte_eth_dev_configure(port, 1, ntxq, &conf) < 0)
        rte_exit(EXIT_FAILURE, "configure failed port %u\n", port);
    int sock = rte_eth_dev_socket_id(port);
    if (rte_eth_rx_queue_setup(port, 0, RX_RING_SIZE, sock, NULL, mp) < 0)
        rte_exit(EXIT_FAILURE, "rx setup failed port %u\n", port);
    for (uint16_t q = 0; q < ntxq; q++)
        if (rte_eth_tx_queue_setup(port, q, TX_RING_SIZE, sock, NULL) < 0)
            rte_exit(EXIT_FAILURE, "tx setup failed port %u q %u\n", port, q);
    if (rte_eth_dev_start(port) < 0)
        rte_exit(EXIT_FAILURE, "start failed port %u\n", port);
}

static void dump_run(const char *prefix, int run_no, uint64_t hz)
{
    char path[512];
    snprintf(path, sizeof(path), "%s_run%02d.csv", prefix, run_no);
    FILE *f = fopen(path, "w");
    if (!f) { printf("cannot open %s\n", path); return; }
    fprintf(f, "pkt_id,tx_ts_ns,rx1_ts_ns,rx2_ts_ns,latency_rx1_ns,latency_rx2_ns,diff_latency_ns\n");
    uint64_t t0 = UINT64_MAX;
    for (uint32_t id = 0; id < MAX_ID; id++) {
        if (seen[0][id] && seen[0][id] < t0) t0 = seen[0][id];
        if (seen[1][id] && seen[1][id] < t0) t0 = seen[1][id];
    }
    uint64_t matched = 0, o0 = 0, o1 = 0;
    for (uint32_t id = 0; id < MAX_ID; id++) {
        uint64_t a = seen[0][id], b = seen[1][id];
        if (a && b) {
            int64_t d = (int64_t)((double)((int64_t)(a-b)) * 1e9 / hz);
            fprintf(f, "%u,0,%lu,%lu,0,0,%ld\n", id,
                    (uint64_t)((double)(a-t0)*1e9/hz),
                    (uint64_t)((double)(b-t0)*1e9/hz), d);
            matched++;
        } else if (a) o0++; else if (b) o1++;
    }
    fclose(f);
    printf("== run %d done: matched=%lu only_p4=%lu only_bypass=%lu reflected=[%lu %lu] -> %s\n",
           run_no, matched, o0, o1, reflected[0], reflected[1], path);
    reflected[0] = reflected[1] = 0;
}

int main(int argc, char **argv)
{
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) rte_exit(EXIT_FAILURE, "EAL init failed\n");
    argc -= ret; argv += ret;

    const char *prefix = "results";
    const char *dmac = NULL;
    int runs = 10, idle_end_s = 5;
    static struct option opts[] = {
        {"prefix", required_argument, 0, 'p'},
        {"runs",   required_argument, 0, 'r'},
        {"dst-mac", required_argument, 0, 'm'},
        {0,0,0,0}};
    int c; optind = 1;
    while ((c = getopt_long(argc, argv, "p:r:m:", opts, NULL)) != -1) {
        if (c=='p') prefix=optarg; else if (c=='r') runs=atoi(optarg);
        else if (c=='m') dmac=optarg;
    }
    if (!dmac) rte_exit(EXIT_FAILURE, "need --dst-mac aa:bb:cc:dd:ee:ff\n");
    struct ether_addr *ea = ether_aton(dmac);
    if (!ea) rte_exit(EXIT_FAILURE, "bad --dst-mac\n");
    memcpy(dst_mac, ea, 6);

    if (rte_eth_dev_count_avail() < 3)
        rte_exit(EXIT_FAILURE, "need 3 ports (2 FPGA + CX5); got %u\n",
                 rte_eth_dev_count_avail());
    if (rte_lcore_count() < 3) rte_exit(EXIT_FAILURE, "need >=3 lcores\n");

    struct rte_mempool *mp = rte_pktmbuf_pool_create("mb", NUM_MBUFS, MBUF_CACHE,
        0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_eth_dev_socket_id(0));
    if (!mp) rte_exit(EXIT_FAILURE, "mempool failed\n");
    mp_refl = rte_pktmbuf_pool_create("mbr", NUM_MBUFS, MBUF_CACHE,
        0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_eth_dev_socket_id(REFL_PORT));
    if (!mp_refl) rte_exit(EXIT_FAILURE, "reflect mempool failed (socket-1 hugepages?)\n");
    printf("reflect pool on socket %d\n", rte_eth_dev_socket_id(REFL_PORT));

    for (uint16_t p = 0; p < FPGA_PORTS; p++) {
        seen[p] = calloc(MAX_ID, sizeof(uint64_t));
        if (!seen[p]) rte_exit(EXIT_FAILURE, "calloc failed\n");
        port_init(p, mp, 1);
    }
    port_init(REFL_PORT, mp, 2);   /* CX5: one TX queue per poller */

    signal(SIGINT, handle_sig); signal(SIGTERM, handle_sig);

    static struct worker_arg wa[FPGA_PORTS];
    unsigned lcore = rte_get_next_lcore(-1, 1, 0);
    for (uint16_t p = 0; p < FPGA_PORTS; p++) {
        wa[p].port = p;
        rte_eal_remote_launch(rx_worker, &wa[p], lcore);
        lcore = rte_get_next_lcore(lcore, 1, 0);
    }

    const uint64_t hz = rte_get_tsc_hz();
    printf("TSC %.3f GHz; kernel-free reflect via port %d, dst=%s; %d runs.\n",
           hz/1e9, REFL_PORT, dmac, runs);

    for (int r = 1; r <= runs && !quit; r++) {
        uint64_t base = marker_count[0] + marker_count[1];
        printf("-- waiting for run %d traffic...\n", r);
        while (!quit && marker_count[0]+marker_count[1] < base+100) rte_delay_ms(200);
        if (quit) break;
        printf("-- run %d receiving...\n", r);
        int idle = 0; uint64_t prev = marker_count[0]+marker_count[1];
        while (!quit && idle < idle_end_s) {
            rte_delay_ms(1000);
            uint64_t now = marker_count[0]+marker_count[1];
            idle = (now == prev) ? idle+1 : 0; prev = now;
        }
        dump_run(prefix, r, hz);
        memset(seen[0], 0, MAX_ID*sizeof(uint64_t));
        memset(seen[1], 0, MAX_ID*sizeof(uint64_t));
    }
    quit = 1; rte_eal_mp_wait_lcore();
    printf("all runs complete. leaving ports up (no close).\n");
    return 0;
}
