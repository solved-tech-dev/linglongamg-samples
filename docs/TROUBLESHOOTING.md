# Troubleshooting

## `GLIBC_2.xx not found`

Check the host C library:

```bash
getconf GNU_LIBC_VERSION
```

If the LinglongAMG package contains `compat/run`, the sample target selects that
launcher automatically:

```bash
make PLATFORM=HIP run
```

For a custom executable or an MPI application, start every process that loads
the library through the runner:

```bash
../LinglongAMG/compat/run ./your-program
mpiexec -n N ../LinglongAMG/compat/run ./your-program
```

The compatibility files are isolated in the package and do not replace or
modify the host C library. The runner must start the top-level process. It
cannot replace glibc after a Python, JVM, MPI rank, or other process has already
started. If the package does not provide a compatible runner, use a package
built for the host glibc baseline.
