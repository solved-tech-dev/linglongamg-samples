#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include "linglongamg.h"
#include "sample_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
_Static_assert(sizeof(linglongamg_real_t)==8,"FP64 library required");
_Static_assert(sizeof(int)==4,"32-bit int required");
typedef struct {int col; double val;} Entry;
static int cmp(const void *a,const void *b){int x=((const Entry*)a)->col,y=((const Entry*)b)->col;return (x>y)-(x<y);}
#define FAIL(s) do{fprintf(stderr,"ERROR: %s\n",s);goto cleanup;}while(0)
#define RUNTIME(call) do{int e=(call);if(e!=0)FAIL(sample_error());}while(0)
#define AMG(call) do{linglongamg_status_t e=(call);if(e!=LINGLONGAMG_STATUS_SUCCESS)FAIL(linglongamg_status_string(e));}while(0)
#define ALLOC(p,n) do{p=calloc((size_t)(n),sizeof(*p));if(!p)FAIL("Out of host memory");}while(0)
#define READ(p,n) do{if(fread(p,sizeof(*p),(size_t)(n),f)!=(size_t)(n))FAIL("Truncated CBD");}while(0)
#define UPLOAD(d,h,n) do{RUNTIME(sample_alloc((void**)&d,(size_t)(n)*sizeof(*h)));RUNTIME(sample_copy(d,h,(size_t)(n)*sizeof(*h),SAMPLE_H2D));}while(0)
int main(int argc,char **argv){
 const char *matrix="spe10.cbd",*config="linglongamg_config.json";
 int rc=1,device=0,check_only=0; double tol=1e-6;
 FILE *f=NULL; int32_t n=0,m=0; int64_t nz64=0,*rp64=NULL; int nnz=0,*rp=NULL,*ci=NULL,*dr=NULL,*dc=NULL;
 double *av=NULL,*b=NULL,*x=NULL,*ba=NULL,*dv=NULL,*db=NULL,*dx=NULL; Entry *entries=NULL;
 linglongamg_handle_t solver=NULL; linglongamg_csr_t *A=NULL;
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--help")||!strcmp(argv[i],"-h")){printf("Usage: %s [-A matrix.cbd] [-c config.json] [--rtol 1e-6] [--device 0] [--check-input]\n",argv[0]);return 0;}
  if(!strcmp(argv[i],"--check-input")){check_only=1;continue;}
  const char *opt=argv[i]; if(++i>=argc)FAIL("Missing option value"); char *end=NULL;
  if(!strcmp(opt,"-A"))matrix=argv[i];
  else if(!strcmp(opt,"-c"))config=argv[i];
  else if(!strcmp(opt,"--rtol")){errno=0;tol=strtod(argv[i],&end);if(errno||end==argv[i]||*end||!isfinite(tol)||tol<=0)FAIL("Invalid tolerance");}
  else if(!strcmp(opt,"--device")){errno=0;long d=strtol(argv[i],&end,10);if(errno||end==argv[i]||*end||d<0||d>INT_MAX)FAIL("Invalid device");device=(int)d;}
  else FAIL("Unknown option");
 }
 {uint16_t v=1;if(*(unsigned char*)&v!=1)FAIL("This CBD reader requires a little-endian host");}
 f=fopen(matrix,"rb");if(!f)FAIL("Cannot open CBD matrix");
 READ(&n,1);READ(&m,1);READ(&nz64,1);
 if(n<=0||m!=n||nz64<=0||nz64>INT_MAX)FAIL("Expected square nonempty CSR within 32-bit index limits");
 {struct stat st;uint64_t expected=16+8*((uint64_t)n+1)+12*(uint64_t)nz64;
 if(fstat(fileno(f),&st)||st.st_size<0||(uint64_t)st.st_size!=expected)FAIL("CBD file length mismatch");}
 nnz=(int)nz64;ALLOC(rp64,(size_t)n+1);ALLOC(rp,(size_t)n+1);ALLOC(ci,nnz);ALLOC(av,nnz);
 READ(rp64,(size_t)n+1);READ(ci,nnz);READ(av,nnz);fclose(f);f=NULL;
 if(rp64[0]!=0||rp64[n]!=nz64)FAIL("Invalid CSR row endpoints");
 int maxrow=0;
 for(int i=0;i<n;i++){if(rp64[i]<0||rp64[i+1]<rp64[i]||rp64[i+1]>nnz)FAIL("Invalid CSR row pointers");rp[i]=(int)rp64[i];int count=(int)(rp64[i+1]-rp64[i]);if(count>maxrow)maxrow=count;}rp[n]=nnz;
 free(rp64);rp64=NULL;ALLOC(entries,maxrow?maxrow:1);
 for(int i=0;i<n;i++){
  int count=rp[i+1]-rp[i];
  for(int j=0;j<count;j++){int k=rp[i]+j;if(ci[k]<0||ci[k]>=n||!isfinite(av[k]))FAIL("Invalid CSR column/value");entries[j].col=ci[k];entries[j].val=av[k];}
  qsort(entries,(size_t)count,sizeof(*entries),cmp);
  for(int j=0;j<count;j++){if(j&&entries[j].col==entries[j-1].col)FAIL("Merge duplicate columns before use");ci[rp[i]+j]=entries[j].col;av[rp[i]+j]=entries[j].val;}
 }
 free(entries);entries=NULL;
 ALLOC(b,n);ALLOC(x,n);ALLOC(ba,n);
 for(int i=0;i<n;i++){long double s=0;for(int k=rp[i];k<rp[i+1];k++)s+=av[k];b[i]=(double)s;if(!isfinite(b[i]))FAIL("Nonfinite RHS");}
 if(check_only){puts("input_check=PASS (no GPU solve performed)");rc=0;goto cleanup;}
 RUNTIME(sample_device(device));
 char license_status[512]={0};
 if(linglongamg_license_check(license_status,sizeof(license_status))!=0){
  printf("Licensee: Unknown\nStatus: Invalid.\n");goto cleanup;
 }
 printf("Licensee: %s\nStatus: Valid.\n",license_status);
 UPLOAD(dr,rp,(size_t)n+1);UPLOAD(dc,ci,nnz);UPLOAD(dv,av,nnz);UPLOAD(db,b,n);UPLOAD(dx,x,n);
 /* Load configuration and validate authorization through the public API. */
 AMG(linglongamg_create_from_config(&solver,config));
 AMG(linglongamg_read_csr(&A,n,n,nnz,dr,dc,dv));
 linglongamg_vec_t rhs={n,db},solution={n,dx};linglongamg_solve_info_t info={0};
 RUNTIME(sample_sync());
 AMG(linglongamg_setup(solver,A));RUNTIME(sample_sync());
 AMG(linglongamg_solve(solver,&rhs,&solution));RUNTIME(sample_sync());
 /* Query must succeed before reading the published solution. */
 AMG(linglongamg_get_solve_info(solver,&info));RUNTIME(sample_sync());
 RUNTIME(sample_copy(x,dx,(size_t)n*sizeof(*x),SAMPLE_D2H));
 RUNTIME(sample_copy(ba,db,(size_t)n*sizeof(*b),SAMPLE_D2H));
 if(memcmp(b,ba,(size_t)n*sizeof(*b)))FAIL("Library modified RHS");
 long double rn=0,bn=0;
 for(int i=0;i<n;i++){if(!isfinite(x[i]))FAIL("Nonfinite solution");long double r=b[i];for(int k=rp[i];k<rp[i+1];k++)r-=(long double)av[k]*x[ci[k]];if(!isfinite(r))FAIL("Nonfinite residual");rn=hypotl(rn,r);bn=hypotl(bn,b[i]);}
 if(!isfinite(rn)||!isfinite(bn))FAIL("Nonfinite norm");
 long double relative=bn>0?rn/bn:NAN;int pass=bn>0?(isfinite(relative)&&relative<tol):rn==0;
 /* Keep successful output identical to the SDK example: license lines
    and the library's own setup/iteration/status output only. */
 if(!pass)fprintf(stderr,"Solution check failed: true_relative_residual=%.2Le tolerance=%.2e\n",relative,tol);
 rc=pass?0:2;
cleanup:
 if(solver)linglongamg_destroy(solver);
 if(A)linglongamg_csr_destroy(A);
 if(dr)sample_free(dr);
 if(dc)sample_free(dc);
 if(dv)sample_free(dv);
 if(db)sample_free(db);
 if(dx)sample_free(dx);
 if(f)fclose(f);
 free(rp64);free(rp);free(ci);free(av);free(b);free(x);free(ba);free(entries);return rc;
}
