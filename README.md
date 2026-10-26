# LinglongAMG Portable C Sample

## Files

```text
LinglongAMG_sample/
|-- linglongamg_test.c          # Portable test program, compiled as C11
|-- sample_runtime.h           # C interface for the runtime adapter
|-- sample_runtime.cpp         # GPU memory and synchronization adapter
|-- Makefile
|-- README.md
|-- spe10.cbd                  # 1,094,421 rows and 7,478,141 nonzeros
`-- linglongamg_config.json
```

This package does not include shared libraries, license files, or AMG kernel source code. The main test program is compiled with a C compiler. The runtime adapter is compiled separately with a C++ compiler (SYCL provides a C++ API), then linked against the existing `liblinglongamg.so` for the selected platform. Do not compile the main program as C++.

## Prerequisites

Use a LinglongAMG SDK that matches your hardware, architecture, and runtime version. The default directory layout is:

```text
work/
|-- LinglongAMG/
|   |-- lib/linglongamg.h
|   |-- lib/liblinglongamg.so
|   `-- ...                    # Keep all SDK authorization and runtime files
`-- LinglongAMG_sample/
```

Configure authorization according to the SDK instructions and retain its complete runtime environment. The sample does not bypass library authorization. A valid license for the selected GPU and any additional authorization files required by the SDK must be available. Paths are configurable; no developer-specific absolute paths are embedded.

Enter the sample directory and load the matching toolchain before building. `AMG_ROOT` defaults to `../LinglongAMG`. Use the header supplied with the same SDK as the library. The sample requires FP64 types and the public `linglongamg_` APIs: `license_check`, `create_from_config`, `read_csr`, `setup`, `solve`, and `get_solve_info`.

## Platform Selection and Build

| PLATFORM | Hardware / toolchain | Arrays passed to the library |
|---|---|---|
| CUDA | NVIDIA; matching CUDA 12.4, 12.9, or 13.1 SDK | CUDA device arrays |
| HIP | Hygon Z100 or BW1000 with matching DTK; AMD with matching ROCm | HIP device arrays |
| HCR | X201 / HPCC 3.5.3.18, aarch64 | HPCC CUDA bridge device arrays |
| MUSA | Moore Threads / MUSA 5.1 | MUSA device arrays |
| SYCL | Intel / oneAPI 2026.1 | SYCL shared USM |
| OPENCL | Lisuan / matching OpenCL SDK | Host arrays; the AMG library manages GPU data |

NVIDIA: load the CUDA environment and set `CUDA_HOME` first.

```bash
make PLATFORM=CUDA CUDA_ROOT="$CUDA_HOME"
```

Z100, BW1000, and AMD: load the matching DTK or ROCm environment so that `hipcc` is available.

```bash
make PLATFORM=HIP HIPCXX=hipcc
```

X201: build in an aarch64 environment with `HPCC_PATH` configured.

```bash
make PLATFORM=HCR HPCC_ROOT="$HPCC_PATH"
```

Moore Threads: load the MUSA environment and set `MUSA_HOME` first.

```bash
make PLATFORM=MUSA MUSA_ROOT="$MUSA_HOME"
```

Intel: load the oneAPI environment first.

```bash
make PLATFORM=SYCL SYCLCXX=icpx
```

Lisuan: load the OpenCL runtime, driver, and library dependencies first.

```bash
make PLATFORM=OPENCL
```

Add `AMG_ROOT=../YourSDK`, `CC=gcc`, or `CXX=g++` as needed. `CC` must be a C compiler for the target platform; do not set it to a C++ driver such as `hipcc` or `icpx`. X201 requires aarch64 object files and an aarch64 SDK. Set `CUDA_LIBDIR` if CUDA libraries are not in `lib64`, or `MUSA_LIBDIR` if MUSA libraries are not in `lib`. For toolchains installed across multiple directories, use `CPPFLAGS`, `LDFLAGS`, and `LIBRARY_PATH` to supply additional search paths.

Each build links one platform's library. A single executable does not support all GPU vendors. For each DTK version, load the matching runtime and link the matching SDK before building. Do not interchange Z100 and BW1000 libraries.

## Run

Keep the vendor runtime environment loaded and add the SDK library directory:

```bash
export LD_LIBRARY_PATH="../LinglongAMG/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./linglongamg_test_CUDA
./linglongamg_test_CUDA -A ./spe10.cbd -c ./linglongamg_config.json --rtol 1e-6 --device 0
```

For other platforms, replace the executable suffix with `HIP`, `HCR`, `MUSA`, `SYCL`, or `OPENCL`. For example:

```bash
./linglongamg_test_HIP -A ./spe10.cbd -c ./linglongamg_config.json
```

The default matrix and configuration are read from the current working directory. Use `-A` and `-c` to specify other paths. On CUDA, HIP, MUSA, and HCR, `--device` selects a GPU by its process-visible ordinal. SYCL uses the GPU selector from the existing SDK sample: keep `--device 0` and use `ONEAPI_DEVICE_SELECTOR` in the oneAPI environment to restrict visible devices when needed. The OpenCL library selects its GPU internally; the sample accepts only `--device 0` for that platform.

To check the CBD input without solving:

```bash
./linglongamg_test_CUDA --check-input
```

This mode does not validate GPU execution, authorization, or numerical convergence. The executable still requires its shared-library dependencies to be loadable.

## Configuration and Solver Workflow

The included configuration uses GMRES(10) with AMG, L1-Jacobi smoothing, and a V-cycle. It specifies 2/2/2 pre/post/coarsest smoothing sweeps, at most 50 levels, a coarse-size threshold of 2, a strong-connection threshold of 0.25, and a solver tolerance of 1e-6. SpMV and vector operations explicitly use `manual` to avoid relying on different vendor sparse operators across platforms. This configuration is not an exact copy of every release package's defaults. Use identical configurations and hardware when comparing historical performance.

1. Read the full-CSR CBD input, validate indices and values, and sort column indices within each row. Duplicate column entries must be merged before use.
2. Construct `b=A*ones` and a zero initial guess, then prepare the arrays required by the platform.
3. Call `linglongamg_create_from_config` to load JSON settings and perform the library's initial authorization check.
4. Call `linglongamg_read_csr` to create the matrix descriptor, then run setup and solve.
5. Call `linglongamg_get_solve_info` after solve completes. Read x only after this call succeeds.
6. Independently compute `||b-Ax||2/||b||2` using the original matrix and b, and verify that b has not changed.

The CBD layout is little-endian: int32 row count, int32 column count, int64 nonzero count, int64 row pointers (row count + 1), int32 column indices, and FP64 values. Indices are zero-based. The matrix must be nonempty and square; indices and the nonzero count must fit within the current int32 API limits. The test program generates b rather than reading it from CBD. JSON RHS defaults do not override the explicitly supplied b.

`--rtol` controls the sample's independent relative L2 residual acceptance threshold; it does not change the JSON solver tolerance. When changing the solver tolerance or norm, also consider the independent acceptance threshold. For a zero RHS, the relative residual is undefined (NaN); acceptance requires an exactly zero absolute residual.

## Output and Exit Codes

Normal output follows the SDK sample: `Licensee: ...` and `Status: Valid.` appear first, followed by the library's own hierarchy, iteration, and final-status output. The sample does not add matrix summaries, setup/solve/query timings, independent residual summaries, or verification-success lines. It does not repeat the library's final status. Timing fields already printed in the library's tables are retained.

Independent residual verification still runs and remains silent on success. On failure, it writes `Solution check failed` and the residual to standard error. Authorization failure prints `Licensee: Unknown` and `Status: Invalid.`, then stops. The separate `--check-input` mode still prints its input-check result.

Exit codes:

- `0`: independent residual verification passed, or input checking passed in `--check-input` mode.
- `2`: the independent residual acceptance criterion was not met.
- `1`: an input, runtime, API, or authorization error occurred.

## Validation Status

The CUDA branch has passed C compilation, C++ adapter compilation, shared-library linking, and input checking with the actual spe10 matrix. The OpenCL host-array adapter has passed compilation. The remaining branches were adapted from the existing platform SDK samples; this portable sample has not yet completed compilation and GPU solve validation across all vendor toolchains. These checks do not establish successful solves on every platform or provide performance results.
