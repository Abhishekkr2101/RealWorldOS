#include <stdio.h>
#include <math.h>

#define MAX_TASKS 10

typedef struct {
    char name[8];
    int period;
    int exec;
} Task;

int gcd(int a, int b) { return b == 0 ? a : gcd(b, a % b); }
int lcm(int a, int b) { return a / gcd(a, b) * b; }

void sort_by_period(Task t[], int n) {
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (t[j].period < t[i].period) {
                Task tmp = t[i]; t[i] = t[j]; t[j] = tmp;
            }
}

double utilization(Task t[], int n) {
    double u = 0;
    for (int i = 0; i < n; i++) u += (double)t[i].exec / t[i].period;
    return u;
}

/* Exact RMS test: R = C + sum(ceil(R/Tj) * Cj) over higher-priority tasks */
int response_time_analysis(Task t[], int n) {
    int all_ok = 1;
    printf("\n  Response-time analysis (RMS priority order):\n");
    for (int i = 0; i < n; i++) {
        int R = t[i].exec, prev = -1;
        printf("   %-3s: R = %d", t[i].name, R);
        while (R != prev && R <= t[i].period) {
            prev = R;
            R = t[i].exec;
            for (int j = 0; j < i; j++)
                R += (int)ceil((double)prev / t[j].period) * t[j].exec;
            if (R != prev) printf(" -> %d", R);
        }
        int ok = R <= t[i].period;
        printf("   (deadline %d) %s\n", t[i].period, ok ? "MET" : "MISSED");
        if (!ok) all_ok = 0;
    }
    return all_ok;
}

/* policy 0 = RMS (static priority), 1 = EDF (earliest absolute deadline) */
int simulate(Task t[], int n, int policy) {
    int H = t[0].period;
    for (int i = 1; i < n; i++) H = lcm(H, t[i].period);

    int remaining[MAX_TASKS] = {0}, deadline[MAX_TASKS] = {0};
    int misses = 0, last = -2, start = 0, idle = 0;

    printf("\n  %s schedule over hyperperiod 0-%d ms:\n  ", policy ? "EDF" : "RMS", H);
    for (int time = 0; time <= H; time++) {
        int run = -1;
        if (time < H) {
            for (int i = 0; i < n; i++) {
                if (time % t[i].period == 0) {
                    if (remaining[i] > 0) {
                        printf("\n  !! %s missed its deadline at t=%d\n  ", t[i].name, time);
                        misses++;
                    }
                    remaining[i] = t[i].exec;
                    deadline[i] = time + t[i].period;
                }
            }
            for (int i = 0; i < n; i++) {
                if (remaining[i] == 0) continue;
                if (run == -1) run = i;
                else if (policy == 1 && deadline[i] < deadline[run]) run = i;
            }
        }
        if (time == H || run != last) {
            if (last != -2 && time > start)
                printf("[%d-%d %s] ", start, time, last == -1 ? "idle" : t[last].name);
            start = time;
            last = run;
        }
        if (time < H) {
            if (run >= 0) remaining[run]--;
            else idle++;
        }
    }
    for (int i = 0; i < n; i++)
        if (remaining[i] > 0) {
            printf("\n  !! %s missed its deadline at t=%d", t[i].name, H);
            misses++;
        }
    printf("\n  Idle time: %d ms   Deadline misses: %d\n", idle, misses);
    return misses;
}

void analyse(const char *title, Task t[], int n) {
    sort_by_period(t, n);
    double U = utilization(t, n);
    double bound = n * (pow(2.0, 1.0 / n) - 1);

    printf("\n==================================================\n");
    printf(" %s\n", title);
    printf("==================================================\n");
    for (int i = 0; i < n; i++)
        printf("  %-3s  T = %3d ms  C = %2d ms  U = %.3f\n",
               t[i].name, t[i].period, t[i].exec, (double)t[i].exec / t[i].period);
    printf("\n  Total utilization U         = %.4f\n", U);
    printf("  Liu-Layland bound n(2^1/n-1) = %.4f\n", bound);
    printf("  RMS bound test: %s\n", U <= bound ? "PASS (guaranteed schedulable)"
                                               : "INCONCLUSIVE (need exact test)");

    int rta = response_time_analysis(t, n);
    int rms_miss = simulate(t, n, 0);
    int edf_miss = simulate(t, n, 1);

    printf("\n  RESULT: RMS -> %s | EDF -> %s (U %s 1)\n",
           (rta && rms_miss == 0) ? "SCHEDULABLE" : "NOT SCHEDULABLE",
           edf_miss == 0 ? "SCHEDULABLE" : "NOT SCHEDULABLE", U <= 1 ? "<=" : ">");
}

int main() {
    Task icu[] = {
        {"T1", 50, 12},    /* heart-rate sensor read     */
        {"T2", 100, 30},   /* ventilator pressure control */
        {"T3", 200, 40}    /* display and alarm update    */
    };
    analyse("ICU controller: original task set", icu, 3);

    Task icu_ecg[] = {
        {"T1", 50, 12},
        {"T2", 100, 30},
        {"T3", 200, 40},
        {"T4", 80, 20}     /* new ECG logging task */
    };
    analyse("ICU controller: after adding T4 (ECG logging)", icu_ecg, 4);

    Task icu_fix[] = {
        {"T1", 50, 12},
        {"T2", 100, 30},
        {"T3", 200, 40},
        {"T4", 100, 20}    /* T4 moved to a harmonic period */
    };
    analyse("Suggested fix: T4 with harmonic period 100 ms", icu_fix, 4);
    return 0;
}
