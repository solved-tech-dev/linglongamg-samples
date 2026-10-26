# Main test is ALWAYS compiled by a C compiler. Only runtime adapter uses C++.
PLATFORM ?= CUDA
AMG_ROOT ?= ../LinglongAMG
CC = cc
CXX = c++
CUDA_ROOT ?= $(CUDA_HOME)
HIPCXX ?= hipcc
SYCLCXX ?= icpx
MUSA_ROOT ?= $(MUSA_HOME)
MUSA_LIBDIR ?= $(MUSA_ROOT)/lib
CUDA_LIBDIR ?= $(CUDA_ROOT)/lib64
HPCC_ROOT ?= $(HPCC_PATH)
OBJDIR := .sample-build/$(PLATFORM)
EXE := linglongamg_test_$(PLATFORM)
CPPFLAGS += -I$(AMG_ROOT)/lib
CFLAGS ?= -O2
CXXFLAGS ?= -O2
LINKFLAGS = -L$(AMG_ROOT)/lib -Wl,-rpath,'$$ORIGIN/../LinglongAMG/lib'
LIBS = -llinglongamg -lm
ADAPTER_FLAGS = -std=c++17 -DSAMPLE_$(PLATFORM)
DRIVER = $(CXX)
ifeq ($(PLATFORM),CUDA)
ifneq ($(strip $(CUDA_ROOT)),)
ADAPTER_FLAGS += -I$(CUDA_ROOT)/include
LINKFLAGS += -L$(CUDA_LIBDIR) -Wl,-rpath-link,$(CUDA_LIBDIR)
endif
LIBS += -lcudart -lcusparse
else ifeq ($(PLATFORM),HIP)
DRIVER = $(HIPCXX)
else ifeq ($(PLATFORM),MUSA)
ADAPTER_FLAGS += -I$(MUSA_ROOT)/include
LINKFLAGS += -L$(MUSA_LIBDIR) -Wl,-rpath-link,$(MUSA_LIBDIR)
LIBS += -lmusart
else ifeq ($(PLATFORM),HCR)
DRIVER = $(HPCC_ROOT)/tools/cu-bridge/bin/cucc
ADAPTER_FLAGS += --hpcc-path=$(HPCC_ROOT) -x cu
LINKFLAGS += --hpcc-path=$(HPCC_ROOT)
export HPCC_PATH = $(HPCC_ROOT)
export CUDA_PATH = $(HPCC_ROOT)/tools/cu-bridge
export CUCC_PATH = $(HPCC_ROOT)/tools/cu-bridge
LIBS += -lcusparse -ldl
else ifeq ($(PLATFORM),SYCL)
DRIVER = $(SYCLCXX)
ADAPTER_FLAGS += -fsycl
LINKFLAGS += -fsycl
else ifeq ($(PLATFORM),OPENCL)
else
$(error PLATFORM must be CUDA, HIP, MUSA, HCR, SYCL or OPENCL)
endif
.PHONY: all run clean FORCE
all: $(EXE)
$(OBJDIR):
	mkdir -p $@
# Recompile when switching SDK/compiler settings, avoiding stale ABI objects.
$(OBJDIR)/main.o: linglongamg_test.c sample_runtime.h FORCE | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -Wall -Wextra -c $< -o $@
$(OBJDIR)/runtime.o: sample_runtime.cpp sample_runtime.h FORCE | $(OBJDIR)
	$(DRIVER) $(CPPFLAGS) $(CXXFLAGS) $(ADAPTER_FLAGS) -c $< -o $@
$(EXE): $(OBJDIR)/main.o $(OBJDIR)/runtime.o
	$(DRIVER) $^ $(LDFLAGS) $(LINKFLAGS) $(LIBS) -o $@
run: $(EXE)
	./$(EXE) $(ARGS)
clean:
	rm -f $(OBJDIR)/main.o $(OBJDIR)/runtime.o $(EXE)
