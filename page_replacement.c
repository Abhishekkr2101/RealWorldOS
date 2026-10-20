#include <stdio.h>
#include <stdbool.h>

#define MAX_FRAMES 10
#define REF_LEN 20

int ref[REF_LEN] = {1, 2, 3, 4, 2, 1, 5, 6, 2, 1, 2, 3, 7, 6, 3, 2, 1, 2, 3, 6};

int find(int frames[], int n, int page) {
    for (int i = 0; i < n; i++) if (frames[i] == page) return i;
    return -1;
}

void print_frames(int frames[], int nf) {
    printf("[");
    for (int i = 0; i < nf; i++) {
        if (frames[i] == -1) printf(" -");
        else printf(" %d", frames[i]);
    }
    printf(" ]");
}

/* algo: 0 = FIFO, 1 = LRU, 2 = OPTIMAL */
int simulate(int algo, int nf, bool verbose) {
    const char *name[] = {"FIFO", "LRU", "OPTIMAL"};
    int frames[MAX_FRAMES], loaded_at[MAX_FRAMES], last_used[MAX_FRAMES];
    int faults = 0;

    for (int i = 0; i < nf; i++) { frames[i] = -1; loaded_at[i] = -1; last_used[i] = -1; }

    if (verbose) printf("\n--- %s with %d frames ---\n", name[algo], nf);

    for (int t = 0; t < REF_LEN; t++) {
        int page = ref[t];
        int pos = find(frames, nf, page);

        if (pos != -1) {
            last_used[pos] = t;
            if (verbose) { printf("  Ref %d: ", page); print_frames(frames, nf); printf("  hit\n"); }
            continue;
        }

        faults++;
        int victim = find(frames, nf, -1);      /* use an empty frame if there is one */
        if (victim == -1) {
            victim = 0;
            for (int i = 1; i < nf; i++) {
                if (algo == 0 && loaded_at[i] < loaded_at[victim]) victim = i;
                if (algo == 1 && last_used[i] < last_used[victim]) victim = i;
            }
            if (algo == 2) {
                int farthest = -1;
                for (int i = 0; i < nf; i++) {
                    int next = REF_LEN + 1;                 /* never used again */
                    for (int k = t + 1; k < REF_LEN; k++)
                        if (ref[k] == frames[i]) { next = k; break; }
                    if (next > farthest) { farthest = next; victim = i; }
                }
            }
        }
        int evicted = frames[victim];
        frames[victim] = page;
        loaded_at[victim] = t;
        last_used[victim] = t;

        if (verbose) {
            printf("  Ref %d: ", page); print_frames(frames, nf);
            if (evicted == -1) printf("  FAULT\n");
            else printf("  FAULT (replaced %d)\n", evicted);
        }
    }
    if (verbose) printf("  Total page faults = %d, fault rate = %.2f\n", faults, (double)faults / REF_LEN);
    return faults;
}

int main() {
    const double mem_ns = 100.0;           /* memory access time      */
    const double fault_ns = 8000000.0;     /* 8 ms page-fault service */

    printf("=== Android Phone: Virtual Memory Under Pressure ===\n");

    /* (a) paging basics: 4 GB RAM, 4 KB pages, 32-bit addresses */
    int offset_bits = 12, page_bits = 32 - offset_bits;
    unsigned int frames_total = 1u << (32 - offset_bits);     /* 4 GB / 4 KB  */
    unsigned int table_bytes = (1u << page_bits) * 4;         /* 4-byte PTEs  */
    printf("\n(a) Offset bits = %d, page-number bits = %d\n", offset_bits, page_bits);
    printf("    Number of frames = %u\n", frames_total);
    printf("    Single-level page table = %u bytes (%u MB) per process\n", table_bytes, table_bytes >> 20);

    unsigned int vaddr = 0x0003A7F4, frame = 0x1F2;
    unsigned int page = vaddr >> 12, offset = vaddr & 0xFFF;
    printf("    VA 0x%08X -> page 0x%X, offset 0x%X -> frame 0x%X -> PA 0x%X\n",
           vaddr, page, offset, frame, (frame << 12) | offset);

    /* (b) page replacement with 4 frames, step by step */
    printf("\n(b) Reference string: ");
    for (int i = 0; i < REF_LEN; i++) printf("%d ", ref[i]);
    printf("\n");
    int f[3];
    for (int a = 0; a < 3; a++) f[a] = simulate(a, 4, true);

    printf("\n    Comparison (page faults)\n    Frames   FIFO   LRU   OPTIMAL\n");
    for (int nf = 3; nf <= 5; nf++)
        printf("    %d        %2d     %2d    %2d\n", nf, simulate(0, nf, false), simulate(1, nf, false), simulate(2, nf, false));

    /* (c) effective access time */
    printf("\n(c) EAT = (1 - p) x %.0f ns + p x %.0f ns\n", mem_ns, fault_ns);
    const char *alg[] = {"FIFO", "LRU", "OPTIMAL"};
    for (int a = 0; a < 3; a++) {
        double p = (double)f[a] / REF_LEN;
        double eat = (1 - p) * mem_ns + p * fault_ns;
        printf("    %-8s p = %.2f  ->  EAT = %.0f ns (%.2f ms)\n", alg[a], p, eat, eat / 1e6);
    }
    double p_steady = 0.001;
    double eat_steady = (1 - p_steady) * mem_ns + p_steady * fault_ns;
    printf("    Steady state p = 0.001 -> EAT = %.1f ns (%.0fx slower)\n", eat_steady, eat_steady / mem_ns);
    double p_max = (0.10 * mem_ns) / (fault_ns - mem_ns);
    printf("    For < 10%% slowdown: p < %.3e (about 1 fault per %.0f accesses)\n", p_max, 1 / p_max);

    /* (d) working-set model and thrashing */
    int delta = 10;
    printf("\n(d) Working set with window delta = %d:\n", delta);
    for (int t = delta; t <= REF_LEN; t += delta) {
        bool seen[16] = {false};
        int wss = 0;
        printf("    WS(t=%d) = {", t);
        for (int k = t - delta; k < t; k++)
            if (!seen[ref[k]]) { seen[ref[k]] = true; wss++; }
        for (int p = 0; p < 16; p++) if (seen[p]) printf(" %d", p);
        printf(" }  WSS = %d %s\n", wss, wss > 4 ? "> 4 frames -> thrashing" : "");
    }

    double ram_gb = 4.0, os_gb = 1.5;
    double ws_game = 1.4, ws_browser = 1.2, ws_whatsapp = 0.3;
    double demand = ws_game + ws_browser + ws_whatsapp, avail = ram_gb - os_gb;
    printf("\n    Total working-set demand D = %.1f GB, available = %.1f GB\n", demand, avail);
    if (demand > avail) {
        printf("    D > available -> THRASHING. Suspending a process...\n");
        printf("    Kill cached browser (%.1f GB): D = %.1f GB %s available -> paging returns to normal\n",
               ws_browser, demand - ws_browser, (demand - ws_browser) <= avail ? "<=" : ">");
    } else {
        printf("    D <= available -> no thrashing\n");
    }
    return 0;
}
