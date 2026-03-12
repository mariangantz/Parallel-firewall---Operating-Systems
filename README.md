# Parallel Firewall

This project represents a C implementation of a parallelized firewall. The main goal was to transform a program with sequential execution into a concurrent one, efficiently utilizing the POSIX threads API (`pthreads`). 

The program simulates the behavior of a real firewall that receives network packets, analyzes them based on predefined filters, and decides whether to accept (`PASS`) or reject (`DROP`) them.

## 📌 Architecture and Synchronization

The application is built on a robust **Producer-Consumer** model, optimized to process packets efficiently and without unnecessarily consuming resources (no *busy waiting*).

### 1. Producer
A main thread is responsible for reading simulated network packets directly from an input file. Once read, it inserts the packets into a shared data structure, ensuring a constant flow of data for the consumers.

### 2. Circular Buffer (Ring Buffer)
To transfer data between the producer and consumers, I implemented a fixed-size Ring Buffer. This data structure acts as a FIFO queue.
* **Synchronization:** Access to the buffer is protected by a mutex (`pthread_mutex_t`) to prevent race conditions.
* **Notifications:** I used condition variables (`pthread_cond_t`) to put threads into a waiting state (`not_empty`, `not_full`). Thus, consumers are awakened only when new packets are available, completely eliminating the *busy waiting* phenomenon.

### 3. Consumers
Consumers are multiple threads (between 1 and 32, configurable via the command line) that fetch packets from the Ring Buffer. To maximize performance, consumers extract packets in **batches**. Each packet is then evaluated by a filtering function that compares the source against a set of permitted IP ranges (`allowed_sources_range`).

### 4. Sorting and Writing Logs
A critical requirement of the project was that the final logs must be written in ascending order based on each packet's *timestamp* as they are processed. Because the threads finish processing in a non-deterministic order, I implemented a local sorting mechanism:
* **Min-Heap:** Consumers insert the resulting logs into a Min-Heap data structure, synchronized with a mutex.
* **Controlled Writing:** Once the number of elements in the heap exceeds the allocated capacity for reordering, the element with the smallest timestamp is extracted and written to a local output buffer.
* **Flush:** The local buffer writes the data to the final file on the disk only when it fills up, thereby reducing costly system calls (`write`).
