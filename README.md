# RealWorldOS
# Real-World OS Problems - Simulations in C

Four real-world scenarios from the **21CSC202J Operating Systems** syllabus, each solved as a C program that runs the scenario and prints the full working.

| # | Scenario | Syllabus topic | File |
|---|----------|----------------|------|
| 1 | ICU patient-monitoring controller | Real-Time CPU Scheduling (RMS, EDF) | `rt_scheduler.c` |
| 2 | Food-delivery order server | Process Synchronization (Producer-Consumer + Readers-Writers) | `food_delivery_sync.c` |
| 3 | Cloud VM host | Deadlock Avoidance, Detection and Recovery | `cloud_deadlock.c` |
| 4 | Android phone under memory pressure | Paging, Page Replacement, Thrashing | `page_replacement.c` |

**Author:** R.K. Krishiv Kumar (RA2511028010156), NWC

---

# 1. Real-Time Scheduling of an ICU Monitoring Controller

A simulation of **Rate-Monotonic Scheduling (RMS)** and **Earliest-Deadline-First (EDF)** for a hospital ICU controller that runs hard real-time periodic tasks on a single core.

---

## 🚀 About the Project
A late ventilator correction or a missed heart-rate sample can endanger a patient, so every task must finish before its next release. The controller runs:

| Task | Function | Period (ms) | Execution (ms) |
|------|----------|-------------|----------------|
| T1 | Heart-rate sensor read | 50 | 12 |
| T2 | Ventilator pressure control | 100 | 30 |
| T3 | Display and alarm update | 200 | 40 |
| T4 *(new)* | ECG waveform logging | 80 | 20 |

### How it Works:
* Computes the CPU utilization `U = Σ C/T` and checks it against the **Liu-Layland bound** `n(2^(1/n) − 1)`.
* If the bound test is inconclusive, runs the exact **response-time analysis** `R = C + Σ ⌈R/Tj⌉·Cj` for every task.
* Simulates both schedulers millisecond by millisecond over the **hyperperiod** (LCM of the periods) and prints the timeline and any deadline misses.
* Repeats the analysis after adding T4, and for a suggested fix where T4 uses a harmonic 100 ms period.

---

## 🛠️ Scheduling Policies Used
1. **Rate-Monotonic (static priority)**: The task with the shorter period always has the higher priority.
2. **Earliest-Deadline-First (dynamic priority)**: The job with the nearest absolute deadline runs first.

## 📊 Result
| Task set | U | RMS | EDF |
|----------|---|-----|-----|
| T1, T2, T3 | 0.74 | Schedulable (R3 = 94 ms) | Schedulable |
| + T4 (80 ms) | 0.99 | **T3 misses its deadline** (R3 = 208 > 200) | Schedulable |
| + T4 (100 ms, harmonic) | 0.94 | Schedulable (R3 = 188 ms) | Schedulable |

---

# 2. Food-Delivery Order Server - Synchronization Simulation in C

A multi-threaded **bounded-buffer + readers-writers** simulation of a food-delivery dispatch server (like Swiggy/Zomato), using POSIX threads (`pthreads`), semaphores (`semaphore.h`) and mutex locks (`pthread_mutex_t`).

---

## 🚀 About the Project
Ready orders for a city zone are kept in a shared circular buffer of **10 slots**. Three kinds of threads share it:
* **Restaurant threads (producers)** put "order ready" events into the buffer.
* **Delivery-partner threads (consumers)** pick orders up.
* **Analytics threads (readers)** read the whole buffer to show a live dashboard, but never modify it.

### How it Works:
* A restaurant waits if all 10 slots are full; a partner waits if no order is ready.
* Several analytics dashboards can read **at the same time**, but never while an order is being added or removed.
* A turnstile semaphore makes threads take turns in arrival order, so continuous dashboard reads cannot **starve** order dispatch.
* At the end, the program checks that every order was delivered exactly once.

---

## 🛠️ Synchronization Mechanisms Used
1. **Empty Semaphore (`sem_t empty`)**: Counts free order slots (initialized to `BUFFER_SIZE`).
2. **Full Semaphore (`sem_t full`)**: Counts orders ready for pickup (initialized to `0`).
3. **Read-Write Semaphore (`sem_t rw_mutex`)**: Held by one restaurant/partner, or by the whole group of readers.
4. **Mutex Lock (`pthread_mutex_t read_lock`)**: Protects the shared `read_count`.
5. **Turnstile Semaphore (`sem_t turnstile`)**: Serves threads in arrival order to prevent starvation.

## ⚠️ Deadlock Note
Producers always call `sem_wait(&empty)` **before** taking the lock. If the order is swapped (lock first, then `wait(empty)`), a producer that finds the buffer full sleeps while holding the lock. The consumer that would free a slot can never get the lock, so the system deadlocks. All four conditions hold: mutual exclusion, hold and wait, no preemption and circular wait.

---

# 3. Cloud VM Host - Deadlock Avoidance and Detection in C

A C program that applies the **Banker's algorithm** to a cloud server hosting 5 customer VMs, then runs **deadlock detection** and picks the **cheapest victim** to recover.

---

## 🚀 About the Project
The hypervisor hands out three resource types: **CPU cores (12)**, **RAM blocks of 4 GB (10)** and **GPUs (6)**. Each customer VM declares its maximum claim at sign-up.

| VM | Allocation | Max | Tier |
|----|------------|-----|------|
| VM1 | 2, 1, 1 | 5, 3, 2 | Premium |
| VM2 | 3, 2, 1 | 5, 4, 2 | Standard |
| VM3 | 2, 2, 1 | 4, 4, 4 | Spot |
| VM4 | 1, 1, 0 | 3, 2, 1 | Standard |
| VM5 | 2, 1, 1 | 6, 3, 3 | Standard |

### How it Works:
* Computes **Available** and **Need = Max − Allocation**, then runs the safety algorithm.
* Handles two live requests with the **resource-request algorithm**: it pretends to allocate, checks safety, then grants or rolls back.
* Switches to **detection mode** with a Request matrix, finds the deadlocked VMs, and preempts the VM with the lowest recovery cost.

---

## 🛠️ Key Features
* **Safety Check**: Prints each step of the safety algorithm and the safe sequence.
* **Request Handling**: VM3's request for (0, 0, 2) GPUs is **denied** because it would leave 0 GPUs and an unsafe state. VM2's request for (1, 1, 0) is **granted**.
* **Deadlock Identification**: Finds the deadlock among **VM1, VM3 and VM5**.
* **Cost-Based Recovery**: `cost = 10 × tier weight + minutes of work lost`. It picks **VM3** (Spot, cost 20) as the victim and verifies that every other VM then completes.

## 📊 Result
* Safe sequence: **< VM2, VM3, VM4, VM5, VM1 >**
* VM3 request → DENY, VM2 request → GRANT
* Deadlocked: {VM1, VM3, VM5} → preempt **VM3**

---

# 4. Android Phone Under Memory Pressure - Page Replacement in C

A simulation of **paging, page replacement (FIFO / LRU / Optimal), effective access time and thrashing** for a gaming phone that starts lagging when many apps are open.

---

## 🚀 About the Project
The phone has **4 GB RAM**, **4 KB pages**, 100 ns memory access and an 8 ms page-fault service time. The game's texture-streaming thread gets **4 frames** and uses the reference string:
`1 2 3 4 2 1 5 6 2 1 2 3 7 6 3 2 1 2 3 6`

### How it Works:
* Works out the address split, the number of frames and the page-table size, and translates one virtual address.
* Runs **FIFO, LRU and Optimal** step by step, printing the frames after every reference, then compares 3, 4 and 5 frames.
* Computes the **effective access time** and the highest fault rate that keeps the slowdown under 10%.
* Applies the **working-set model** to the thread and to the whole phone to show why it thrashes.

---

## 🛠️ Key Features
* **Address Translation**: 20-bit page number + 12-bit offset; VA `0x0003A7F4` → PA `0x1F27F4`.
* **Page Faults (4 frames)**: FIFO **14**, LRU **10**, Optimal **8**.
* **EAT**: `EAT = (1 − p)·100 ns + p·8 ms`. A fault rate of only 0.001 already makes memory **81× slower**; the limit is **p < 1.25 × 10⁻⁶**.
* **Thrashing Detection**: The working sets total 2.9 GB but only 2.5 GB is free. Killing the cached browser (1.2 GB) brings the demand down to 1.7 GB, which is what Android's low-memory killer does.

---

# ▶️ How to Run

```bash
gcc rt_scheduler.c -o rt_scheduler -lm
gcc food_delivery_sync.c -o food_delivery_sync -pthread
gcc cloud_deadlock.c -o cloud_deadlock
gcc page_replacement.c -o page_replacement

./rt_scheduler
./food_delivery_sync
./cloud_deadlock
./page_replacement
```

On Windows (MinGW), run `gcc <file>.c -o <file>.exe` (add `-lpthread` for the food-delivery program), then run the `.exe`.
