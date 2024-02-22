# Requirements
You need an LLVM>=17.x installation. Minimal build as follows:
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
mkdir build
CC=clang CXX=clang++ cmake ..
make
```
No need to install the OMPT plugin, Taskgrind reads directly to 'valgrind/taskgrind/runtimes-tools/ompt/build'


# Usage
## Dev mode
```
cd valgrind/taskgrind
make
../vg-in-place --tool=taskgrind tests/test.exe
```

The function `void task_fini(void)` in `task.c` is run at the end of execution: add manually analyses here.
Some code may be buggy at the moment, don't blindly trust generated data structures.
You may want to dump generated data structures to dot files with
```
../vg-in-place --tool=taskgrind --dump tests/test.exe
```

# TODO (PRIORITY) LIST BEFORE MAKING TASKGRIND PUBLICLY AVAILABLE
1) Add the OMPT Plugin to the installation, and have Taskgrind loading it.
2) Optimize the 'spmt.h' data structure. Probably rewrite it entirely keeping the same interfaces

# Testing with dataracebench
The project had been modded to 'dataracebench/' to support taskgrind
```
cd dataracebench/
./check-data-races.sh --taskgrind C
```
Original repo is here (https://github.com/LLNL/dataracebench)
