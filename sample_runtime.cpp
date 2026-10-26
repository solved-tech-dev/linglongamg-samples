// Runtime adapter only; the test program is compiled as C11.
#include "sample_runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
static char error_text[1024]="runtime operation failed";
const char *sample_error(void){return error_text;}
#if defined(SAMPLE_SYCL)
#include <sycl/sycl.hpp>
static sycl::queue &queue(){static sycl::queue q(sycl::gpu_selector_v,sycl::property::queue::in_order{});return q;}
#define TRY(call) try {call; return 0;} catch(const std::exception &e){std::snprintf(error_text,sizeof(error_text),"%s",e.what());return 1;}
int sample_device(int d){if(d!=0){std::snprintf(error_text,sizeof(error_text),"SYCL: select one GPU with ONEAPI_DEVICE_SELECTOR; use --device 0");return 1;}TRY((void)queue())}
int sample_alloc(void **p,size_t n){TRY(*p=sycl::malloc_shared(n,queue());if(!*p)throw std::bad_alloc())}
int sample_copy(void *d,const void *s,size_t n,int){TRY(queue().memcpy(d,s,n).wait_and_throw())}
int sample_sync(void){TRY(queue().wait_and_throw())}
int sample_free(void *p){TRY(sycl::free(p,queue()))}
#elif defined(SAMPLE_OPENCL)
// This LinglongAMG OpenCL ABI accepts host arrays, as in its SDK example.
int sample_device(int d){if(d!=0){std::snprintf(error_text,sizeof(error_text),"OpenCL SDK selects its GPU internally; --device must be 0");return 1;}return 0;}
int sample_alloc(void **p,size_t n){*p=std::malloc(n);return *p?0:1;}
int sample_copy(void *d,const void *s,size_t n,int){std::memcpy(d,s,n);return 0;}
int sample_sync(void){return 0;} // SDK operations complete before returning host results.
int sample_free(void *p){std::free(p);return 0;}
#else
#if defined(SAMPLE_HIP)
#include <hip/hip_runtime.h>
#define RT(n) hip##n
#elif defined(SAMPLE_MUSA)
#include <musa_runtime.h>
#define RT(n) musa##n
#else // CUDA or HPCC CUDA bridge
#include <cuda_runtime.h>
#define RT(n) cuda##n
#endif
static int checked(RT(Error_t) e){if(e==RT(Success))return 0;std::snprintf(error_text,sizeof(error_text),"%s",RT(GetErrorString)(e));return 1;}
int sample_device(int d){return checked(RT(SetDevice)(d));}
int sample_alloc(void **p,size_t n){return checked(RT(Malloc)(p,n));}
int sample_copy(void *d,const void *s,size_t n,int direction){return checked(RT(Memcpy)(d,s,n,direction==SAMPLE_H2D?RT(MemcpyHostToDevice):RT(MemcpyDeviceToHost)));}
int sample_sync(void){return checked(RT(DeviceSynchronize)());}
int sample_free(void *p){return checked(RT(Free)(p));}
#endif
