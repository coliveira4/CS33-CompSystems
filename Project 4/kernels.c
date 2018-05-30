

/********************************************************
 * Kernels to be optimized for the CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include <pthread.h>
#include <semaphore.h>
#include <algorithm>
#include <string.h>
#include "helper.h"

pthread_t threads[32];
pthread_barrier_t barrier;
/* 
 * Please fill in the following team struct 
 */
user_t user = {
    (char*)  "204803448",            /* UID */
    (char*)  "Christina Oliveira",          /* Full name */
    (char*)  "coliveira@ucla.edu",  /* Email */
};

//If you worked with anyone in this project, please indicate that here:
// I worked with  "... , ... ".

// Of course, your entire file should be hand-written by you.  You are free to
// look at tutorials and academic literature for radix-sort based sorting.  You
// are not allowed to look at or copy from implementations on online code repositories
// like github.


//  You should modify only this file, but feel free to modify however you like!


/*
 * setup - This function runs one time, and will not be timed.
 *         You can do whatever initialization you need here, but
 *         it is not required -- don't use if you don't want to.  ^_^
 */
void setup() {
  //..

  // So, in my experiments, it take fewer cycles for each run if we 
  // waste some time doing absolute nothing in particular
  // at the begining of the program.  It might be because of Intel's Turbo
  // mode and DVFS somehow??  TBH IDK, but I would leave this in if I were
  // you.
  for(int i = 0; i < 50000;++i) {
     do_nothing(i);
  }
}

/******************************************************
 * Your different versions of the singlethread kernel go here
 ******************************************************/
bool kvp_compare(kvp lhs, kvp rhs) { 
  return lhs.key < rhs.key; 
}

/*
 * naive_singlethread - The naive baseline version of singlethread 
 */
char naive_singlethread_descr[] = "naive_singlethread: Naive baseline implementation";
void naive_singlethread(int dim, kvp *src, kvp *dst) 
{
    //This is the built-in stable sort if you want to try it
    //memcpy(dst, src, dim*sizeof(kvp));
    //std::stable_sort(dst, dst+dim, kvp_compare);
    //return;

    int log_radix=8; //Radix of radix-sort is 2^8
    int iters=(sizeof(unsigned int)*8/log_radix);

    // 256 buckets for 2^8 bins, one for each iteration 
    unsigned long long buckets[256+1][iters];
    unsigned long long sum[256+1][iters];

    for(int iter = 0; iter < iters; ++iter) {
      for(int i = 0; i < bucket_size(log_radix); ++i) {
        buckets[i][iter]=0;
        sum[i][iter]=0;
      }

      //1. Generate the bucket count
      for(int i = 0; i < dim; ++i) {
        int index = gen_shift(src[i].key,iter*log_radix,
                              (bucket_size(log_radix)-1))+1;
        buckets[index][iter]++;
      }

      //2. Perform scan
      for(int i = 1; i < bucket_size(log_radix); ++i) {
        sum[i][iter] = buckets[i][iter] + sum[i-1][iter];
      }

      //3. Move Data items
      for(int i = 0; i < dim; ++i) {
        int index = gen_shift(src[i].key,iter*log_radix,
                              bucket_size(log_radix)-1);
        int out_index = sum[index][iter];
        move_kvp(dst,src,i,out_index);
        sum[index][iter]++;
      }

      // Move dest back to source
      for(int i = 0; i < dim; ++i) {
        move_kvp(src,dst,i,i);
      }

    }
}


/*
 * singlethread - Your current working version of singlethread. 
 * IMPORTANT: This is the version you will be graded on
 */
char singlethread_descr[] = "singlethread: Current working version";
void singlethread(int dim, kvp *src, kvp *dst) 

{

  //  int log_radix=8; //Radix of radix-sort is 2^8
  int bsize = 256;
  int iters=4;

  unsigned long long buckets[iters][257];
  unsigned long long sum[iters][257];

  int index = 0;
  int out_index = 0;
  int multeight = 0;
  int bsize_minus_one = bsize - 1;
  int i = 0;
  for(int iter = 0; iter < iters; ++iter) {
    memset(buckets, 0, sizeof(buckets));
    memset(sum, 0, sizeof(sum));
    multeight = iter << 3;
   
    //1. Generate the bucket count
    for(i = 0; i < dim; ++i) {
      buckets[iter][(((src[i].key >> multeight)&(bsize_minus_one))+1)]++;
    }
    
    //2. Perform scan
    for(i = 1; i < bsize; ++i) {
      sum[iter][i] = buckets[iter][i] + sum[iter][i-1];
    }
  
    //3. Move Data items
    for(i = 0; i < dim; ++i) {
      index  = ((src[i].key >> multeight) & (bsize_minus_one));
      out_index = sum[iter][index];
      dst[out_index].key = src[i].key;
      dst[out_index].value = src[i].value;
      sum[iter][index]++;
    }
    
    // Move dest back to source
    for(i = 0; i < dim; ++i) {
      src[i].key = dst[i].key;
      src[i].value = dst[i].value;
    }
    
  }
 //   return naive_singlethread(dim,src,dst);
}


/********************************************************************* 
 * register_singlethread_functions - Register all of your different versions
 *     of the singlethread kernel with the driver by calling the
 *     add_singlethread_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_singlethread_functions() {
    add_singlethread_function(&naive_singlethread, naive_singlethread_descr);
    add_singlethread_function(&singlethread, singlethread_descr);
    /* ... Register additional test functions here */
}



// ----------------------- do multi-threaded versions here ------------------


// I'm kind of cheating to pass global variables to my thread function
// There are nicer ways to do this, but w/e
int gdim;
kvp* gsrc;
kvp* gdst;

void *do_sort(void* threadid) {
  // printf("Hello from thread %d\n", tid);
  int bsize = 256;
  long tid = (long)threadid;
  int dim = gdim;
  kvp* src = gsrc;
  kvp* dst = gdst;
  int n_threads = 2;
  unsigned long long buckets[n_threads][bsize+1];
    unsigned long long sum[n_threads][bsize+1];
  int elements = dim / n_threads;
  int start_index = tid * elements;
  int end_index = start_index + elements;
    
    
  for(int iter = 0; iter < 4; ++iter) {

    int multeight = iter << 3;
        
    //initialize buckets
    memset(buckets, 0, sizeof(buckets));
    memset(sum, 0, sizeof(sum));
        
    pthread_barrier_wait(&barrier);
        
    //1. Generate the bucket count
    for(int i = start_index ; i < end_index; ++i) {
      //src[i]>>8
      //  int index = gen_shift(src[i].key,multeight,(bsize-1));
    
      buckets[tid][(src[i].key >> multeight) & 255]++;
  }


    //2. Perform scan-local prefix sum, 255
    for(int i = 1; i < bsize; ++i) {
      buckets[tid][i] += buckets[tid][i-1];
      //buckets?
    }
        
    pthread_barrier_wait(&barrier);
        
    //compute global sum
        
    for(int i = 1; i < (n_threads-1); ++i) {
	  for(int j = 0; j < bsize; ++j){
	    buckets[i][j] += buckets[i-1][j];
	  }
     }
       
	for(int i = 0; i < bsize; ++i){
	  for(tid=(n_threads-2);tid >=0;--tid){	  
	      buckets[tid][i] += (buckets[tid+1][i-1] - buckets[tid][i-1]);
     	  }
	}
     

    pthread_barrier_wait(&barrier);
    //3. Move Data items
    for(int i = start_index; i < end_index; ++i) {
      int out_index = --buckets[tid][(src[i].key >> multeight) & 255];
      dst[out_index].key = src[i].key;
      dst[out_index].value = src[i].value;
    }
        
    // Move dest back to source
    for(int i = start_index; i < end_index; ++i) {
      //  move_kvp(src,dst,i,i);
      src[i].key = dst[i].key;
      src[i].value = dst[i].value;   
 }        
    pthread_barrier_wait(&barrier);
        
  }
  return 0;
}





/* 
 * naive_multithread - The naive baseline version of multithread 
 */
char naive_multithread_descr[] = "naive_multithread: Naive baseline implementation";
void naive_multithread(int dim, kvp *src, kvp *dst) 
{
    gdim=dim;
    gsrc=src;
    gdst=dst;
    // pthread_barrier_init(&barrier, NULL, 4);

    //call one thread to do our work -- I'm sure that will make things go faster
    // pthread_create(&threads[0], NULL, do_sort, (void *)0 /*tid*/);

    // void** ret=0;
    // pthread_join(threads[0],ret);
     return naive_singlethread(dim,src,dst);
}
/* 
 * multithread - Your current working version of multithread
 * IMPORTANT: This is the version you will be graded on
 */
char multithread_descr[] = "multithread: Current working version";
void multithread(int dim, kvp *src, kvp *dst) 
{
  gdim=dim;
  gsrc=src;
  gdst=dst;
  if(dim<=16384){
    return singlethread(dim,src,dst);
  }




  pthread_barrier_init(&barrier, NULL, 2);

  pthread_create(&threads[0], NULL, do_sort, (void *)0 /*tid*/);
  pthread_create(&threads[1], NULL, do_sort, (void *)1 /*tid*/);
  //   pthread_create(&threads[2], NULL, do_sort, (void *)2 /*tid*/);
  //  pthread_create(&threads[3], NULL, do_sort, (void *)3 /*tid*/);
    
  void** ret=0;
  pthread_join(threads[0],ret);
  pthread_join(threads[1],ret);
  //  pthread_join(threads[2],ret);
  //  pthread_join(threads[3],ret);
    
}

/*********************************************************************
 * register_multithread_functions - Register all of your different versions
 *     of the multithread kernel with the driver by calling the
 *     add_multithread_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_multithread_functions() 
{
    add_multithread_function(&naive_multithread, naive_multithread_descr);   
    add_multithread_function(&multithread, multithread_descr);   
    /* ... Register additional test functions here */
}

