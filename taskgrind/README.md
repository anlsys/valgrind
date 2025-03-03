# Requirements
You need an LLVM>=17.x installation. You can build LLVM as follows
```
cmake -S llvm -B build -G "Unix Makefiles" -DLLVM_TARGETS_TO_BUILD=X86 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=on -DLLVM_ENABLE_PROJECTS="clang;openmp" -DCMAKE_INSTALL_PREFIX=PREFIX
```

# Install

## Building Valgrind and Taskgrind
Refer to 'Building and installing it' section from Valgrind README

## Building Taskgrind OMPT Plugin
You need a Clang>=17.x for this step.
```
cd valgrind/taskgrind/runtimes-tools/ompt/
mkdir build && cd build
CC=clang CXX=clang++ cmake ..
make
```
Installation of the OMPT plugin is not supported yet

# Usage
## Dev mode
```
cd valgrind/taskgrind
make
make -C tests
../vg-in-place --tool=taskgrind tests/...
```

The function `void task_fini(void)` in `task.c` run at the end of execution: add analyses here.
Some code may be buggy at the moment, don't blindly trust generated data structures.
You may want to dump generated data structures to dot files with
```
../vg-in-place --tool=taskgrind --dump tests/test.exe
```
# References
Taskgrind: Heavyweight Dynamic Binary Instrumentation for Parallel Programs Analysis
https://hal.science/hal-04814885v1/document

# Testing with dataracebench
The project had been modded to 'dataracebench/' to support taskgrind
```
cd dataracebench/
./check-data-races.sh --taskgrind C
```
Original repo is here (https://github.com/LLNL/dataracebench)
