# OS Mini Project – Multi-Container Runtime

## 1. Team Information

### Team Member 1
- **Name:** Anisha Nukul Niranjan  
- **SRN:** PES1UG24CS064

  ### Team Member 2
- **Name:** Anushka P  
- **SRN:** PES1UG24CS075


## 2. Build, Load, and Run Instructions

### Build the Project
```bash
cd ~/OS-Jackfruit/boilerplate
make
```

### Load Kernel Module
```bash
sudo insmod monitor.ko
```

### Verify Device
```bash
ls -l /dev/container_monitor
```

### Start Supervisor
```bash
sudo ./engine supervisor
```

### Create Container Filesystems
```bash
cp -a ./rootfs-base ./rootfs-alpha
cp -a ./rootfs-base ./rootfs-beta
```

### Start Containers
```bash
sudo ./engine start alpha sleep
sudo ./engine start beta sleep
```

### List Running Containers
```bash
sudo ./engine ps
```

### View Logs
```bash
sudo ./engine logs alpha
```

### Stop Containers
```bash
sudo ./engine stop alpha
sudo ./engine stop beta
```

### Check Processes
```bash
ps aux | grep engine
```

### Unload Module
```bash
sudo rmmod monitor
```





## 3. Demo with Screenshots

### 1. Multi-container Supervision
<img width="1600" height="1000" alt="image" src="https://github.com/user-attachments/assets/dfccb6af-5d8c-4257-9e63-1fa16493967c" />
<img width="1600" height="166" alt="image" src="https://github.com/user-attachments/assets/aa11e2d8-5d2f-46be-854e-d67bc6fb6543" />

The supervisor has started. Two containers (alpha and beta) successfully started with unique PIDs, demonstrating multi-container supervision under a single supervisor process.

---

### 2. Metadata Tracking
<img width="1600" height="82" alt="image" src="https://github.com/user-attachments/assets/3f257254-7542-4572-a945-8612c007c484" />
Output of engine ps displaying tracked container metadata, including container name (beta) and its associated process ID (PID 33638)

---

### 3. Bounded-Buffer Logging
<img width="1600" height="429" alt="image" src="https://github.com/user-attachments/assets/a164b439-c4b5-473f-a2e5-f3cccbf1bd51" />

Log output demonstrating the bounded-buffer logging pipeline, where logs are continuously produced and consumed asynchronously, indicating producer–consumer behavior.

---

### 4. CLI and IPC
<img width="1600" height="192" alt="image" src="https://github.com/user-attachments/assets/8a4ae3e2-1475-4525-ab84-e772e6651d9c" />

CLI commands issued to start a container and query its status, with the supervisor responding by creating the container and returning metadata, demonstrating command-based interaction and IPC between user-space and the supervisor.

---

### 5. Soft-limit Warning
<img width="1600" height="390" alt="image" src="https://github.com/user-attachments/assets/9e7fc178-487e-4f45-91ab-55c345d1834c" />
Kernel log output showing a soft-limit warning for container gamma, where memory usage exceeded the configured soft threshold, demonstrating early warning behavior before enforcement.

---

### 6. Hard-limit Enforcement
<img width="1600" height="334" alt="image" src="https://github.com/user-attachments/assets/ca443401-8a3f-4402-80f8-4327ac6d5102" />
dmesg output showing hard-limit enforcement, where container gamma exceeds its memory limit and is terminated by the kernel module, demonstrating strict memory control.

---

### 7. Scheduling Experiment
<img width="1600" height="966" alt="image" src="https://github.com/user-attachments/assets/471cdb9e-b893-43dc-a58e-2247279baca9" />

Output of workload programs (cpu_hog and io_pulse) demonstrating different system behaviors—CPU-intensive computation vs I/O activity—used for scheduling experiments and performance comparison.

---

### 8. Clean Teardown
<img width="1600" height="177" alt="image" src="https://github.com/user-attachments/assets/79ac2038-05a3-4805-bf23-ea2d4fe39d15" />

<img width="1600" height="191" alt="image" src="https://github.com/user-attachments/assets/0c2f3007-15a4-43a5-a815-dd42d0169507" />

Containers (alpha, beta, gamma) are stopped using the engine. The ps aux | grep engine output shows no active container processes, confirming that containers were terminated successfully and no zombie processes remain.


## 4. Engineering Analysis

This section connects the implementation of the project to core Operating System concepts, explaining how and why these mechanisms are used.

---

### 1. Isolation Mechanisms

The runtime achieves process and filesystem isolation using Linux namespaces and filesystem techniques.  

- **PID namespaces** ensure each container has its own process ID space, preventing processes from seeing or interfering with others.  
- **UTS namespaces** isolate hostname and domain information.  
- **Mount namespaces** provide separate filesystem views.  

Filesystem isolation is achieved using `chroot` (or similar mechanisms like `pivot_root`), which restricts the container’s root directory to its own filesystem. This ensures processes inside the container cannot access files outside their environment.

However, all containers still share the same host kernel. This means kernel resources such as CPU scheduling, memory management, and system calls are common across all containers, making containers lightweight compared to full virtual machines.

---

### 2. Supervisor and Process Lifecycle

A long-running supervisor process is used to manage all containers. It acts as the parent process responsible for creating, tracking, and controlling container processes.

When a container is started, the supervisor:
- forks a child process  
- executes the container workload  

This establishes a parent-child relationship.

The supervisor also:
- handles process termination using signals  
- reaps child processes to prevent zombie processes  

Additionally, metadata such as container name, PID, and status is tracked by the supervisor, allowing commands like `ps` to display active containers.

---

### 3. IPC, Threads, and Synchronization

The project uses multiple IPC mechanisms for communication between user-space and kernel-space, as well as within the runtime.

- **ioctl** is used for communication between user-space programs and the kernel module  

For logging, a **bounded-buffer (producer-consumer) model** is used:
- Producer → writes logs  
- Consumer → reads logs  

This shared buffer introduces potential race conditions when accessed concurrently.

To prevent this:
- **Mutexes** ensure mutual exclusion  
- **Condition variables** coordinate producer-consumer behavior (e.g., waiting when buffer is full or empty)  

This guarantees thread-safe and efficient communication.

---

### 4. Memory Management and Enforcement

Memory usage is monitored using **RSS (Resident Set Size)**, which represents the actual physical memory used by a process.

Two types of limits are implemented:

- **Soft limit:** Generates a warning when exceeded but allows execution to continue  
- **Hard limit:** Strictly enforced; the process is terminated if exceeded  

Enforcement is implemented in **kernel space** because:
- the kernel has direct access to process memory information  
- it can enforce limits reliably  

User-space enforcement alone would not be secure or accurate, as processes could bypass it.

---

### 5. Scheduling Behavior

The project demonstrates Linux scheduling behavior using different workloads:

- CPU-bound → `cpu_hog`  
- Memory-bound → `memory_hog`  
- I/O-bound → `io_pulse`  

The Linux scheduler (**CFS - Completely Fair Scheduler**) distributes CPU time fairly among processes.

- CPU-bound processes consume more CPU time  
- I/O-bound processes frequently yield CPU, improving responsiveness  

This highlights key scheduling goals:

- **Fairness:** Equal CPU distribution across processes  
- **Responsiveness:** Quick handling of I/O tasks  
- **Throughput:** Maximizing overall system performance  

---

### Boilerplate Contents

The `boilerplate/` folder contains starter files for:

- User-space runtime (`engine.c`)  
- Kernel-space memory monitor (`monitor.c`)  
- Shared ioctl definitions (`monitor_ioctl.h`)  
- Test workloads (`cpu_hog.c`, `memory_hog.c`, `io_pulse.c`)  
- Build system (`Makefile`)  

This structure provides a foundation for implementing container runtime features and experimenting with OS concepts.




## 5. Design Decisions and Tradeoffs

---

###  Namespace Isolation

- **Design Choice:**  
  Used Linux namespaces (PID, UTS, mount) along with `chroot` for process and filesystem isolation.

- **Tradeoff:**  
  This approach is lightweight and fast compared to virtual machines, but all containers still share the same host kernel, so isolation is not as strong as full virtualization.

- **Justification:**  
  It is efficient and sufficient for demonstrating OS-level isolation concepts while keeping the implementation simple.

---

###  Supervisor Architecture

- **Design Choice:**  
  Implemented a single long-running supervisor process to manage all containers.

- **Tradeoff:**  
  Centralized control simplifies management, but if the supervisor crashes, all container management is affected.

- **Justification:**  
  A single supervisor makes it easier to track processes, handle lifecycle events, and prevent zombie processes.

---

###  IPC and Logging

- **Design Choice:**  
  Used `ioctl` for user-kernel communication and a bounded-buffer (producer-consumer) model for logging.

- **Tradeoff:**  
  This provides efficient communication but requires careful synchronization to avoid race conditions.

- **Justification:**  
  It demonstrates core OS concepts like IPC and synchronization while ensuring controlled and structured logging.

---

###  Kernel Monitor

- **Design Choice:**  
  Implemented memory monitoring and enforcement inside a kernel module.

- **Tradeoff:**  
  Kernel-level implementation is powerful but riskier, as bugs can affect overall system stability.

- **Justification:**  
  Only the kernel has accurate and secure access to process memory usage, making it the correct place for enforcement.

---

###  Scheduling Experiments

- **Design Choice:**  
  Used different workloads (`cpu_hog`, `memory_hog`, `io_pulse`) to observe scheduling behavior.

- **Tradeoff:**  
  These simple workloads may not fully represent real-world scenarios.

- **Justification:**  
  They are sufficient to clearly demonstrate scheduling differences such as CPU-bound vs I/O-bound behavior.





  ## 6. Scheduler Experiment Results

---
<img width="1600" height="262" alt="image" src="https://github.com/user-attachments/assets/c82d31ba-4c22-4202-8d4c-714fce062079" />
<img width="1600" height="831" alt="image" src="https://github.com/user-attachments/assets/100658db-e32b-42f3-a86b-f9fc82e7ec24" />

### Raw Observations

We ran two different workloads inside containers:

- **alpha** → running `cpu_hog` (CPU-intensive)  
- **beta** → running `io_pulse` (I/O-based / lighter workload)  

From the output:

- Both containers started successfully with different PIDs  
- `engine ps` shows active containers (metadata tracking works)  
- Logs (`engine logs alpha`) show continuous system activity  
- `top` output shows system CPU usage and running processes  

---

### Comparison

| Container | Workload  | Behavior Observed              | CPU Usage |
|----------|-----------|------------------------------|----------|
| alpha    | cpu_hog   | Continuous execution          | Higher   |
| beta     | io_pulse  | Intermittent / lighter usage  | Lower    |

---

### Analysis of Results

From the `top` output:

- CPU usage is mostly idle (~98–99%), meaning workloads are light  
- Active processes are scheduled one at a time (`1 running, rest sleeping`)  
- `cpu_hog` runs continuously when scheduled  
- `io_pulse` runs in bursts and yields CPU frequently  

This demonstrates how the Linux scheduler works:

- **CPU-bound processes** (like `cpu_hog`) use CPU continuously  
- **I/O-bound processes** (like `io_pulse`) give up CPU often  
- The scheduler switches between processes to maintain fairness  

---

### Key Scheduling Insights

- **Fairness:** Both containers get CPU time  
- **Responsiveness:** I/O tasks do not block the system  
- **Throughput:** Multiple workloads run without crashing or starvation  

---

### Conclusion

The experiment demonstrates that the Linux scheduler efficiently manages different workload types. CPU-intensive processes dominate CPU usage, while I/O-based processes remain responsive. This confirms the behavior of the **Completely Fair Scheduler (CFS)** in balancing performance and fairness.
