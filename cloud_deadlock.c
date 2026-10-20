#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define N 5     /* virtual machines                 */
#define M 3     /* resource types: CPU, RAM, GPU    */

const char *res_name[M] = {"CPU", "RAM", "GPU"};
const char *vm_name[N] = {"VM1", "VM2", "VM3", "VM4", "VM5"};

int total[M]           = {12, 10, 6};
int allocation[N][M]   = {{2,1,1}, {3,2,1}, {2,2,1}, {1,1,0}, {2,1,1}};
int max_claim[N][M]    = {{5,3,2}, {5,4,2}, {4,4,4}, {3,2,1}, {6,3,3}};

/* recovery cost data: tier weight (Premium 3, Standard 2, Spot 1) and minutes of work lost */
const char *tier[N]    = {"Premium", "Standard", "Spot", "Standard", "Standard"};
int tier_weight[N]     = {3, 2, 1, 2, 2};
int work_lost_min[N]   = {45, 20, 10, 15, 30};

void print_vec(const int v[M]) { printf("(%d, %d, %d)", v[0], v[1], v[2]); }

bool leq(const int a[M], const int b[M]) {
    for (int j = 0; j < M; j++) if (a[j] > b[j]) return false;
    return true;
}

/* Banker's safety algorithm. Returns true if safe and fills seq[]. */
bool is_safe(int avail[M], int alloc[N][M], int need[N][M], int seq[N], bool verbose) {
    int work[M];
    bool finish[N] = {false};
    int k = 0;
    memcpy(work, avail, sizeof(work));

    bool progress;
    do {
        progress = false;
        for (int i = 0; i < N; i++) {
            if (finish[i]) continue;
            bool ok = leq(need[i], work);
            if (verbose) {
                printf("   %s Need ", vm_name[i]); print_vec(need[i]);
                printf(" <= Work "); print_vec(work);
                printf(" ? %s", ok ? "YES" : "no");
            }
            if (ok) {
                for (int j = 0; j < M; j++) work[j] += alloc[i][j];
                finish[i] = true;
                seq[k++] = i;
                progress = true;
                if (verbose) { printf("  -> finishes, Work = "); print_vec(work); }
            }
            if (verbose) printf("\n");
        }
    } while (progress);
    return k == N;
}

void print_sequence(int seq[N]) {
    printf("< ");
    for (int i = 0; i < N; i++) printf("%s ", vm_name[seq[i]]);
    printf(">\n");
}

void request_resources(int vm, int req[M]) {
    int avail[M], alloc[N][M], need[N][M], seq[N];

    for (int j = 0; j < M; j++) {
        int used = 0;
        for (int i = 0; i < N; i++) used += allocation[i][j];
        avail[j] = total[j] - used;
    }
    memcpy(alloc, allocation, sizeof(alloc));
    for (int i = 0; i < N; i++)
        for (int j = 0; j < M; j++) need[i][j] = max_claim[i][j] - alloc[i][j];

    printf("\n--- %s requests ", vm_name[vm]); print_vec(req); printf(" ---\n");
    if (!leq(req, need[vm])) { printf("   ERROR: request exceeds declared maximum claim\n"); return; }
    if (!leq(req, avail))    { printf("   Not enough free resources -> %s must WAIT\n", vm_name[vm]); return; }
    printf("   Request <= Need and Request <= Available. Pretending to allocate...\n");

    for (int j = 0; j < M; j++) {
        avail[j] -= req[j];
        alloc[vm][j] += req[j];
        need[vm][j] -= req[j];
    }
    printf("   New Available = "); print_vec(avail); printf("\n");

    if (is_safe(avail, alloc, need, seq, true)) {
        printf("   RESULT: State is SAFE -> request GRANTED. Safe sequence: ");
        print_sequence(seq);
    } else {
        printf("   RESULT: State is UNSAFE -> request DENIED, %s must wait (allocation rolled back)\n", vm_name[vm]);
    }
}

void detect_and_recover(int request[N][M]) {
    int avail[M], work[M];
    bool finish[N];

    for (int j = 0; j < M; j++) {
        int used = 0;
        for (int i = 0; i < N; i++) used += allocation[i][j];
        avail[j] = total[j] - used;
    }
    memcpy(work, avail, sizeof(work));
    for (int i = 0; i < N; i++) finish[i] = false;

    printf("\n--- Deadlock detection (Request matrix, no max claims) ---\n");
    bool progress;
    do {
        progress = false;
        for (int i = 0; i < N; i++) {
            if (finish[i]) continue;
            if (leq(request[i], work)) {
                for (int j = 0; j < M; j++) work[j] += allocation[i][j];
                finish[i] = true;
                progress = true;
                printf("   %s request ", vm_name[i]); print_vec(request[i]);
                printf(" can be met -> completes, Work = "); print_vec(work); printf("\n");
            }
        }
    } while (progress);

    int deadlocked = 0;
    for (int i = 0; i < N; i++) if (!finish[i]) deadlocked++;
    if (deadlocked == 0) { printf("   No deadlock detected.\n"); return; }

    printf("\n   Deadlock Detected! Deadlocked VMs:\n");
    for (int i = 0; i < N; i++)
        if (!finish[i]) {
            printf("    -> %s holds ", vm_name[i]); print_vec(allocation[i]);
            printf(", waiting for "); print_vec(request[i]); printf("\n");
        }

    printf("\n--- Recovery: choose cheapest victim to preempt ---\n");
    printf("   Cost = 10 x tier weight + minutes of work lost since checkpoint\n");
    int victim = -1, best = 1 << 30;
    for (int i = 0; i < N; i++) {
        if (finish[i]) continue;
        int cost = 10 * tier_weight[i] + work_lost_min[i];
        printf("   %s  %-8s  weight %d  lost %2d min  -> cost %d\n",
               vm_name[i], tier[i], tier_weight[i], work_lost_min[i], cost);
        if (cost < best) { best = cost; victim = i; }
    }
    printf("   Recommended victim: %s (cost %d)\n", vm_name[victim], best);

    /* verify the deadlock is broken after preempting the victim */
    for (int j = 0; j < M; j++) work[j] += allocation[victim][j];
    finish[victim] = true;
    printf("\n   After preempting %s, Work = ", vm_name[victim]); print_vec(work); printf("\n");
    do {
        progress = false;
        for (int i = 0; i < N; i++) {
            if (finish[i] || !leq(request[i], work)) continue;
            for (int j = 0; j < M; j++) work[j] += allocation[i][j];
            finish[i] = true;
            progress = true;
            printf("   %s completes, Work = ", vm_name[i]); print_vec(work); printf("\n");
        }
    } while (progress);
    printf("   Deadlock resolved. %s is restarted from its last checkpoint.\n", vm_name[victim]);
}

int main() {
    int avail[M], need[N][M], seq[N];

    printf("=== Cloud VM Host: Deadlock Avoidance and Detection ===\n");
    printf("Total resources (CPU cores, RAM blocks, GPUs): "); print_vec(total); printf("\n");

    for (int j = 0; j < M; j++) {
        int used = 0;
        for (int i = 0; i < N; i++) used += allocation[i][j];
        avail[j] = total[j] - used;
    }
    printf("Available: "); print_vec(avail); printf("\n\n");
    printf("VM    Allocation   Max          Need\n");
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) need[i][j] = max_claim[i][j] - allocation[i][j];
        printf("%s   ", vm_name[i]); print_vec(allocation[i]);
        printf("    "); print_vec(max_claim[i]);
        printf("    "); print_vec(need[i]); printf("\n");
    }

    printf("\n--- (a) Banker's safety algorithm ---\n");
    if (is_safe(avail, allocation, need, seq, true)) {
        printf("   RESULT: System is in a SAFE state. Safe sequence: ");
        print_sequence(seq);
    } else {
        printf("   RESULT: System is UNSAFE.\n");
    }

    int req_vm3[M] = {0, 0, 2};
    request_resources(2, req_vm3);

    int req_vm2[M] = {1, 1, 0};
    request_resources(1, req_vm2);

    int request[N][M] = {{7,0,0}, {0,0,0}, {0,0,4}, {0,1,0}, {0,7,0}};
    detect_and_recover(request);

    return 0;
}
