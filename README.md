# S-meandric enumeration

C++20 companion code for the preprint [*S-meandric permutations and tangency polynomials*](http://arxiv.org/). It calculates, for $1\le n\le 24$:

| Number | Counts |
|---|---|
| $g_n$ | Sum-indecomposable s-meandric permutations |
| $s_n$ | All s-meandric permutations |
| $N_n$ | Distinct nonzero tangency polynomials |
| $O_n$ | Distinct tangency polynomials of meandric permutations |

The calculations use two independent algorithms: one for $g_n$ and $s_n$, and the other for $N_n$ and $O_n$. [ALGORITHM.md](ALGORITHM.md) contains their detailed description.

## Build and run

Use a C++20 compiler on a 64-bit little-endian machine; no external C++ libraries are needed. With GCC:

```sh
mkdir build
g++ -std=c++20 -O3 -march=native -pthread "Source files/main.cpp" "Source files/permutations.cpp" "Source files/tangencies.cpp" -o build/meandric
./build/meandric 8
```

The output is CSV:

```text
n,g_n,s_n,N_n,O_n
1,1,1,1,1
2,1,2,2,1
3,3,6,4,2
```

The full command is `meandric n [scratch [partitions [threads]]]`.

- `n`: Maximum order, from 1 to 24. The program calculates all orders from 1 through `n`. Default: `8`. Large orders require substantial RAM and disk space.
- `scratch`: Directory for temporary working files. The program stores intermediate enumeration states there to limit RAM use and deletes files when they are no longer needed. Default: `scratch` in the current directory. Use a dedicated empty directory for each run; for large orders, place it on a local SSD outside synchronized folders.
- `partitions`: Number of parts processed separately at each enumeration step. More partitions reduce RAM use but require repeated scans of the intermediate data. Default: `0`, which automatically chooses between 1 and 32 partitions. For a fixed number, use a positive power of two: `1`, `2`, `4`, `8`, etc.
- `threads`: Number of parallel worker threads. Default: all hardware threads. Use a positive integer; `1` runs serially. Workers share the state tables, so adding threads does not multiply their memory use.
- `--polynomials`: Use after `n`, in place of the remaining parameters, to calculate only $N_n$ and $O_n$. For example: `./build/meandric 24 --polynomials`.

Layer progress goes to standard error; redirect standard output to save the CSV.

For example, to calculate all four sequences through order 24 with automatic partitioning and 32 threads:

```sh
./build/meandric 24 /path/to/ssd/scratch 0 32 > s_meandric_numbers.csv
```

## Measured performance

The table below shows measurements for a full run through order 24 on an AMD Ryzen 9 9950X. It was built with GCC 16.1.0 and the flags above, using automatic partitioning and 32 threads.

| Elapsed time | Peak process memory | Peak scratch space |
|---|---|---|
| 5 h 36 min 10 s | 21.8 GiB | 790.2 GB |

The output from this run is in [s_meandric_numbers.csv](s_meandric_numbers.csv).

## Reading the code

Start with [Source files/main.cpp](<Source files/main.cpp>).

| File | Purpose |
|---|---|
| [Source files/frontier.hpp](<Source files/frontier.hpp>) | Frontier state and vertex transitions for $g_n$ |
| [Source files/state_storage.hpp](<Source files/state_storage.hpp>) | Packed state keys and compact hash tables |
| [Source files/permutations.cpp](<Source files/permutations.cpp>) | Disk layers and shared partition tables |
| [Source files/tangency_profiles.hpp](<Source files/tangency_profiles.hpp>) | Signed-factor insertion and polynomial expansion |
| [Source files/tangencies.cpp](<Source files/tangencies.cpp>) | Enumeration of $N_n,O_n$ |
| [Source files/numbers.hpp](<Source files/numbers.hpp>) | Count types and function declarations |
