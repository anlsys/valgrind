#gcc
#-march=armv8.2-a+sve -msve-vector-bits=512 -mtune=a64fx -O3 -ffast-math
#llvm
#-march=armv8.2-a+sve -msve-vector-bits=512 -mtune=a64fx -ffast-math -O3 -Ofast
#
#LLVM: -Ofast -ffast-math -march=armv8-a+sve -ffp-contract=on -mllvm aarch64-sve-vector-bits-min=512 -mllvm -aarch64-sve-vector-bits-max=512

# Common code
COMMON_CFLAGS=-I common/ -DWALL -O0 -g
#COMMON_CFLAGS+=-Wno-unused-parameter -Wno-unused-variable
#COMMON_CFLAGS+=-Wno-comment -Wno-cast-function-type -Wno-format-overflow -Wno-literal-suffix
#COMMON_CFLAGS+=-march=native -mcpu=native -mtune=native
COMMON_CFLAGS+=-Wall -ffast-math
#COMMON_CFLAGS+=-DTRACE
#COMMON_CFLAGS+=-DTRACE_COLOR
#COMMON_CFLAGS+=-DTRACE_LABEL
COMMON_LDFLAGS=
COMMON_SRC_FILES=compute_residual.cpp dump_matlab_matrix.cpp generate_matrix.cpp \
			HPC_Sparse_Matrix.cpp main.cpp  make_local_matrix.cpp mytimer.cpp \
			read_HPC_row.cpp YAML_Doc.cpp YAML_Element.cpp
COMMON_SRC=$(addprefix common/,$(COMMON_SRC_FILES))

help:
	@echo "usage: make [seq|omp|omp-mpi|mpc|mpc-mpi-ext|omp-task|omp-task-mpi]"

##################################
# --------- SEQUENTIAL --------- #
##################################
SEQ_CXX=g++
SEQ_CFLAGS=-Wall -Wextra
SEQ_LDFLAGS=
SEQ_SRC=$(COMMON_SRC) omp/ddot.cpp omp/exchange_externals.cpp \
				omp/HPCCG.cpp omp/HPC_sparsemv.cpp omp/waxpby.cpp
SEQ_OBJ=$(SEQ_SRC:.cpp=.seq.o)
SEQ_TARGET=hpccg-seq

%.seq.o: %.cpp
	$(SEQ_CXX) $(COMMON_CFLAGS) $(SEQ_CFLAGS) -o $@ -c $<

$(SEQ_TARGET): $(SEQ_OBJ)
	$(SEQ_CXX) $(COMMON_CFLAGS) $(SEQ_CFLAGS) $(SEQ_OBJ) $(COMMON_LDFLAGS) $(SEQ_LDFLAGS) -o $(SEQ_TARGET)

seq: $(SEQ_TARGET)

##################################
# ----------- OMP WS ----------- #
##################################
#OMP_CXX=g++
OMP_CXX=clang++
OMP_CFLAGS=-Wall -Wextra -DUSING_OMP -fopenmp
OMP_LDFLAGS=
OMP_LDFLAGS+=-L/vol0003/ra010013/data/a04427/llvm-project/install-arm/lib
OMP_SRC=$(COMMON_SRC) omp/ddot.cpp omp/exchange_externals.cpp \
				omp/HPCCG.cpp omp/HPC_sparsemv.cpp omp/waxpby.cpp
OMP_OBJ=$(OMP_SRC:.cpp=.omp.o)
OMP_TARGET=hpccg-omp-$(OMP_CXX)

%.omp.o: %.cpp
	$(OMP_CXX) $(COMMON_CFLAGS) $(OMP_CFLAGS) -o $@ -c $<

$(OMP_TARGET): $(OMP_OBJ)
	$(OMP_CXX) $(COMMON_CFLAGS) $(OMP_CFLAGS) $(OMP_OBJ) $(COMMON_LDFLAGS) $(OMP_LDFLAGS) -o $(OMP_TARGET)

omp: $(OMP_TARGET)

##################################
# -------- OMP WS MPI ---------- #
##################################
OMP_MPI_CXX=clang++#mpc_cxx
OMP_MPI_MPICXX=I_MPI_CXX=$(OMP_MPI_CXX) MPICH_CXX=$(OMP_MPI_CXX) OMPI_CXX=$(OMP_MPI_CXX) mpicxx
OMP_MPI_CFLAGS=-Wall -Wextra -DUSING_MPI -DUSING_OMP -fopenmp
#OMP_MPI_CFLAGS+=-fno-mpc-privatize
OMP_MPI_LDFLAGS=
OMP_MPI_LDFLAGS+=-L/vol0003/ra010013/data/a04427/llvm-project/install-arm/lib
OMP_MPI_SRC=$(COMMON_SRC) omp/ddot.cpp omp/exchange_externals.cpp \
				omp/HPCCG.cpp omp/HPC_sparsemv.cpp omp/waxpby.cpp
OMP_MPI_OBJ=$(OMP_MPI_SRC:.cpp=.omp-mpi.o)
OMP_MPI_TARGET=hpccg-omp-mpi-$(OMP_MPI_CXX)

%.omp-mpi.o: %.cpp
	$(OMP_MPI_MPICXX) $(COMMON_CFLAGS) $(OMP_MPI_CFLAGS) -o $@ -c $<

$(OMP_MPI_TARGET): $(OMP_MPI_OBJ)
	$(OMP_MPI_MPICXX) $(COMMON_CFLAGS) $(OMP_MPI_CFLAGS) $(OMP_MPI_OBJ) $(COMMON_LDFLAGS) $(OMP_MPI_LDFLAGS) -o $(OMP_MPI_TARGET)

omp-mpi: $(OMP_MPI_TARGET)

##################################
# --------- OMP TASK ----------- #
##################################
OMP_TASK_CXX=clang++
#OMP_TASK_CXX=g++
#OMP_TASK_CXX=mpc_cxx -cc=g++
#OMP_TASK_CXX=mpc_cxx -cc=clang++
OMP_TASK_CFLAGS=-Wall -Wextra -DUSING_OMP -fopenmp
#OMP_TASK_CFLAGS+=-Ofast -ffast-math -march=armv8-a+sve -ffp-contract=on -mllvm -aarch64-sve-vector-bits-max=512
#OMP_TASK_CFLAGS+=-march=armv8.2-a+sve -msve-vector-bits=512 -mtune=a64fx -O3 -ffast-math
#OMP_TASK_CFLAGS+=-march=armv8.2-a+sve -msve-vector-bits=512 -mtune=a64fx -ffast-math -O3 -Ofast
OMP_TASK_LDFLAGS=
OMP_TASK_LDFLAGS+=-L/vol0003/ra010013/data/a04427/llvm-project/install-arm/lib
OMP_TASK_SRC=$(COMMON_SRC) omp-task/HPCCG.cpp
OMP_TASK_OBJ=$(OMP_TASK_SRC:.cpp=.omp-task.o)
OMP_TASK_TARGET=hpccg-omp-task-$(OMP_TASK_CXX)

%.omp-task.o: %.cpp
	$(OMP_TASK_CXX) $(COMMON_CFLAGS) $(OMP_TASK_CFLAGS) -o $@ -c $<

$(OMP_TASK_TARGET): $(OMP_TASK_OBJ)
	$(OMP_TASK_CXX) $(COMMON_CFLAGS) $(OMP_TASK_CFLAGS) $(OMP_TASK_OBJ) $(COMMON_LDFLAGS) $(OMP_TASK_LDFLAGS) -o $(OMP_TASK_TARGET)

omp-task: $(OMP_TASK_TARGET)

##################################
# ------- OMP TASK MPI --------- #
##################################
OMP_TASK_MPI_CXX=clang++
#OMP_TASK_MPI_CXX=g++
#OMP_TASK_MPI_CXX="mpc_cxx -cc=g++"
#OMP_TASK_MPI_CXX="mpc_cxx -cc=clang++"
OMP_TASK_MPI_MPICXX=I_MPI_CXX=$(OMP_TASK_MPI_CXX) MPICH_CXX=$(OMP_TASK_MPI_CXX) OMPI_CXX=$(OMP_TASK_MPI_CXX) mpicxx
OMP_TASK_MPI_CFLAGS=-Wall -Wextra -DUSING_MPI -DUSING_OMP -fopenmp -g
#OMP_TASK_MPI_CFLAGS+=-DUSING_OMPI
OMP_TASK_MPI_CFLAGS+=-DUSING_MPI_DETACH=1
OMP_TASK_MPI_LDFLAGS=
OMP_TASK_MPI_LDFLAGS+=-ldetach -lrt
#OMP_TASK_MPI_LDFLAGS+=-lmpiP
#OMP_TASK_MPI_CFLAGS+=-march=armv8.2-a+sve -msve-vector-bits=512 -mtune=a64fx -O3 -ffast-math
#OMP_TASK_MPI_CFLAGS+=-march=armv8.2-a+sve -msve-vector-bits=512 -mtune=a64fx -ffast-math -O3 -Ofast
OMP_TASK_MPI_LDFLAGS+=-L/vol0003/ra010013/data/a04427/llvm-project/install-arm/lib

OMP_TASK_MPI_SRC=$(COMMON_SRC) omp-task/HPCCG.cpp
OMP_TASK_MPI_OBJ=$(OMP_TASK_MPI_SRC:.cpp=.omp-task-mpi.o)
#OMP_TASK_MPI_TARGET=hpccg-omp-task-mpi-$(OMP_TASK_MPI_CXX)
OMP_TASK_MPI_TARGET=hpccg-omp-task-mpi

%.omp-task-mpi.o: %.cpp
	$(OMP_TASK_MPI_MPICXX) $(COMMON_CFLAGS) $(OMP_TASK_MPI_CFLAGS) -o $@ -c $<

$(OMP_TASK_MPI_TARGET): $(OMP_TASK_MPI_OBJ)
	$(OMP_TASK_MPI_MPICXX) $(COMMON_CFLAGS) $(OMP_TASK_MPI_CFLAGS) $(OMP_TASK_MPI_OBJ) $(COMMON_LDFLAGS) $(OMP_TASK_MPI_LDFLAGS) -o $(OMP_TASK_MPI_TARGET)

omp-task-mpi: $(OMP_TASK_MPI_TARGET)

##################################
# ---------- OSS TASK ---------- #
##################################
OSS_CXX=mcxx
OSS_CFLAGS=-Wall -Wextra -DUSING_OMPSS --ompss-2
OSS_CFLAGS+=-Wno-parentheses -Wno-unused-local-typedefs -Wno-unused-parameter
OSS_CFLAGS+=--keep-all-files
OSS_LDFLAGS=
OSS_SRC=$(COMMON_SRC) oss/HPCCG.cpp
OSS_OBJ=$(OSS_SRC:.cpp=.oss.o)
OSS_TARGET=hpccg-oss

%.oss.o: %.cpp
	$(OSS_CXX) $(COMMON_CFLAGS) $(OSS_CFLAGS) -o $@ -c $<

$(OSS_TARGET): $(OSS_OBJ)
	$(OSS_CXX) $(COMMON_CFLAGS) $(OSS_CFLAGS) $(OSS_OBJ) $(COMMON_LDFLAGS) $(OSS_LDFLAGS) -o $(OSS_TARGET)

oss: $(OSS_TARGET)

##################################
# ------- OSS_MPI TASK --------- #
##################################
OSS_MPI_CXX=mcxx
OSS_MPI_MPICXX=I_MPI_CXX=$(OSS_MPI_CXX) MPICH_CXX=$(OSS_MPI_CXX) OMPI_CXX=$(OSS_MPI_CXX) mpicxx
OSS_MPI_CFLAGS=-Wall -Wextra -DUSING_MPI -DUSING_OMPSS --ompss-2 -DUSING_OMPI
OSS_MPI_CFLAGS+=-Wno-parentheses -Wno-unused-local-typedefs -Wno-unused-parameter
OSS_MPI_CFLAGS+=-I $(TAMPI_HOME)/include
OSS_MPI_CFLAGS+=--keep-all-files
OSS_MPI_LDFLAGS=-L $(TAMPI_HOME)/lib -ltampi
OSS_MPI_SRC=$(COMMON_SRC) oss/HPCCG.cpp
OSS_MPI_OBJ=$(OSS_MPI_SRC:.cpp=.oss-mpi.o)
OSS_MPI_TARGET=hpccg-oss-mpi

%.oss-mpi.o: %.cpp
	$(OSS_MPI_MPICXX) $(COMMON_CFLAGS) $(OSS_MPI_CFLAGS) -o $@ -c $<

$(OSS_MPI_TARGET): $(OSS_MPI_OBJ)
	$(OSS_MPI_MPICXX) $(COMMON_CFLAGS) $(OSS_MPI_CFLAGS) $(OSS_MPI_OBJ) $(COMMON_LDFLAGS) $(OSS_MPI_LDFLAGS) -o $(OSS_MPI_TARGET)

oss-mpi: $(OSS_MPI_TARGET)

##################################
# ---------- MPC TASK ---------- #
##################################
MPC_CXX=mpc_cxx -cc=g++
#MPC_CXX=mpc_cxx -cc=clang++
MPC_CFLAGS =-Wall -Werror -Wextra -DUSING_OMP -DUSING_MPC -fopenmp
MPC_CFLAGS+=-DDEPENDENCY_PREALLOC -DDEPENDENCY_BLOCK -DDEPENDENCY_FILTERING
MPC_CFLAGS+=-DUSE_PERSISTENT_TASK
MPC_CFLAGS+=#-DCUSTOM_HASH -DHASH_SBS
MPC_CFLAGS+=-march=armv8.2-a+sve -msve-vector-bits=512 -mtune=a64fx -O3 -ffast-math
#MPC_CFLAGS+=-march=armv8.2-a+sve -msve-vector-bits=512 -mtune=a64fx -ffast-math -O3 -Ofast
#MPC_CFLAGS+=-Wno-parentheses -Wno-unused-local-typedefs -Wno-unused-parameter
#MPC_CFLAGS+=-fno-mpc-privatize
MPC_T_SRC=$(COMMON_SRC) mpc/HPCCG.cpp
MPC_T_OBJ=$(MPC_T_SRC:.cpp=.mpc-t.o)
MPC_T_TARGET=hpccg-mpc-gcc13-persistent

%.mpc-t.o: %.cpp
	$(MPC_CXX) $(COMMON_CFLAGS) $(MPC_CFLAGS) -o $@ -c $<

$(MPC_T_TARGET): $(MPC_T_OBJ)
	$(MPC_CXX) $(COMMON_CFLAGS) $(MPC_CFLAGS) $(MPC_T_OBJ) $(COMMON_LDFLAGS) $(MPC_LDFLAGS) -o $(MPC_T_TARGET)

mpc: $(MPC_T_TARGET)

##################################
# ------ MPC TASKWAIT ---------- #
##################################
MPC_TW_SRC=$(COMMON_SRC) mpc/HPCCG.taskwait.cpp
MPC_TW_OBJ=$(MPC_TW_SRC:.cpp=.mpc-tw.o)
MPC_TW_TARGET=hpccg-mpc-tw

%.mpc-tw.o: %.cpp
	$(MPC_CXX) $(COMMON_CFLAGS) $(MPC_CFLAGS) -o $@ -c $<

$(MPC_TW_TARGET): $(MPC_TW_OBJ)
	$(MPC_CXX) $(COMMON_CFLAGS) $(MPC_CFLAGS) $(MPC_TW_OBJ) $(COMMON_LDFLAGS) $(MPC_LDFLAGS) -o $(MPC_TW_TARGET)

mpc-tw: $(MPC_TW_TARGET)

##################################
# ------- MPC_BSC TASK --------- #
##################################
MPC_BSC_CXX="mpc_cxx -cc=g++"
MPC_BSC_CFLAGS=-Wall -Werror -Wextra -DUSING_OMP -DUSING_MPC -DUSING_OMPI -fopenmp
MPC_BSC_CFLAGS+=-Wno-parentheses -Wno-unused-local-typedefs -Wno-unused-parameter
MPC_BSC_LDFLAGS=
MPC_BSC_SRC=$(COMMON_SRC) mpc-bsc/HPCCG.cpp
MPC_BSC_OBJ=$(MPC_BSC_SRC:.cpp=.mpc-bsc.o)
MPC_BSC_TARGET=hpccg-mpc-bsc

%.mpc-bsc.o: %.cpp
	$(MPC_BSC_CXX) $(COMMON_CFLAGS) $(MPC_BSC_CFLAGS) -o $@ -c $<

$(MPC_BSC_TARGET): $(MPC_BSC_OBJ)
	$(MPC_BSC_CXX) $(COMMON_CFLAGS) $(MPC_BSC_CFLAGS) $(MPC_BSC_OBJ) $(COMMON_LDFLAGS) $(MPC_BSC_LDFLAGS) -o $(MPC_BSC_TARGET)

mpc-bsc: $(MPC_BSC_TARGET)

##################################
# ------- MPC_MPI TASK --------- #
##################################
MPC_MPI_CXX="mpc_cxx -cc=g++"
MPC_MPI_MPICXX=mpc_cxx
#MPC_MPI_MPICXX=I_MPI_CXX=$(MPC_MPI_CXX) MPICH_CXX=$(MPC_MPI_CXX) OMPI_CXX=$(MPC_MPI_CXX) mpicxx
MPC_MPI_CFLAGS=-Wall -Werror -Wextra -DUSING_MPI -DUSING_OMP -DUSING_MPC -fopenmp 
MPC_MPI_CFLAGS+=-DDEPENDENCY_PREALLOC -DDEPENDENCY_BLOCK -DDEPENDENCY_FILTERING
MPC_MPI_LDFLAGS=
MPC_MPI_SRC=$(COMMON_SRC) mpc/HPCCG.cpp
MPC_MPI_OBJ=$(MPC_MPI_SRC:.cpp=.mpc-mpi.o)
MPC_MPI_TARGET=hpccg-mpc-mpi-trace

%.mpc-mpi.o: %.cpp
	$(MPC_MPI_MPICXX) $(COMMON_CFLAGS) $(MPC_MPI_CFLAGS) -o $@ -c $<

$(MPC_MPI_TARGET): $(MPC_MPI_OBJ)
	$(MPC_MPI_MPICXX) $(COMMON_CFLAGS) $(MPC_MPI_CFLAGS) $(MPC_MPI_OBJ) $(COMMON_LDFLAGS) $(MPC_MPI_LDFLAGS) -o $(MPC_MPI_TARGET)

mpc-mpi: $(MPC_MPI_TARGET)

########################################
# ---- MPC-OpenMP + Open MPI TASK ---- #
########################################
MPC_MPI_EXT_CXX="mpc_cxx -cc=clang++"
#MPC_MPI_EXT_MPICXX=mpc_cxx
MPC_MPI_EXT_MPICXX=I_MPI_CXX=$(MPC_MPI_EXT_CXX) MPICH_CXX=$(MPC_MPI_EXT_CXX) OMPI_CXX=$(MPC_MPI_EXT_CXX) mpicxx
MPC_MPI_EXT_CFLAGS =-Wall -Werror -Wextra -fopenmp
MPC_MPI_EXT_CFLAGS+=-DUSING_MPI -DUSING_OMP -DUSING_MPC -DUSING_TAMPI
#MPC_MPI_EXT_CFLAGS+=-DUSING_OMPI
MPC_MPI_EXT_CFLAGS+=-DDEPENDENCY_PREALLOC -DDEPENDENCY_BLOCK -DDEPENDENCY_FILTERING
#MPC_MPI_EXT_CFLAGS+=-DUSE_PERSISTENT_TASK
#MPC_MPI_EXT_CFLAGS+=-fno-mpc-privatize
MPC_MPI_EXT_LDFLAGS=
MPC_MPI_EXT_SRC=$(COMMON_SRC) mpc/HPCCG.cpp
MPC_MPI_EXT_OBJ=$(MPC_MPI_EXT_SRC:.cpp=.mpc-mpi-ext.o)
MPC_MPI_EXT_TARGET=hpccg-mpc-mpi-ext-prealloc-block-filter-trace

%.mpc-mpi-ext.o: %.cpp
	$(MPC_MPI_EXT_MPICXX) $(COMMON_CFLAGS) $(MPC_MPI_EXT_CFLAGS) -o $@ -c $<

$(MPC_MPI_EXT_TARGET): $(MPC_MPI_EXT_OBJ)
	$(MPC_MPI_EXT_MPICXX) $(COMMON_CFLAGS) $(MPC_MPI_EXT_CFLAGS) $(MPC_MPI_EXT_OBJ) $(COMMON_LDFLAGS) $(MPC_MPI_EXT_LDFLAGS) -o $(MPC_MPI_EXT_TARGET)

mpc-mpi-ext: $(MPC_MPI_EXT_TARGET)

############################################
# ---- MPC-OpenMP + Open MPI TASKWAIT ---- #
############################################
MPC_TW_MPI_EXT_SRC=$(COMMON_SRC) mpc/HPCCG.taskwait.cpp
MPC_TW_MPI_EXT_OBJ=$(MPC_TW_MPI_EXT_SRC:.cpp=.mpc-tw-mpi-ext.o)
MPC_TW_MPI_EXT_TARGET=hpccg-mpc-tw-mpi-ext

%.mpc-tw-mpi-ext.o: %.cpp
	$(MPC_MPI_EXT_MPICXX) $(COMMON_CFLAGS) $(MPC_MPI_EXT_CFLAGS) -o $@ -c $<

$(MPC_TW_MPI_EXT_TARGET): $(MPC_TW_MPI_EXT_OBJ)
	$(MPC_MPI_EXT_MPICXX) $(COMMON_CFLAGS) $(MPC_MPI_EXT_CFLAGS) $(MPC_TW_MPI_EXT_OBJ) $(COMMON_LDFLAGS) $(MPC_MPI_EXT_LDFLAGS) -o $(MPC_TW_MPI_EXT_TARGET)

mpc-tw-mpi-ext: $(MPC_TW_MPI_EXT_TARGET)

###################################
# ----------- utils ------------- #
###################################
clean:
	rm -rf $(SEQ_OBJ) $(OMP_OBJ) $(OMP_MPI_OBJ) $(OMP_TASK_OBJ) $(OMP_TASK_MPI_OBJ) $(OSS_OBJ) $(OSS_MPI_OBJ) $(MPC_T_OBJ) $(MPC_TW_OBJ) $(MPC_MPI_OBJ) $(MPC_BSC_OBJ) $(MPC_MPI_EXT_OBJ) $(MPC_TW_MPI_EXT_OBJ)

dclean:
	rm -rf *.yaml *.btr *.o *.json mcxx_*.cpp traces* mpc-omp-task-trace-*

fclean: clean dclean
	rm -rf $(SEQ_TARGET) $(OMP_TARGET) $(OMP_MPI_TARGET) $(OMP_TASK_TARGET) $(OMP_TASK_MPI_TARGET) $(OSS_TARGET) $(OSS_MPI_TARGET) $(MPC_T_TARGET) $(MPC_TW_TARGET) $(MPC_MPI_TARGET) $(MPC_BSC_TARGET) $(MPC_MPI_EXT_TARGET) $(MPC_TW_MPI_EXT_TARGET)

re: fclean all
