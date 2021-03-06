#include <unistd.h>
#include <stdint.h>

extern uint8_t *probe_buf;
extern uint64_t cur_probe_space;
extern uint64_t signal_idx;

uint64_t try_decrypt(void);
__uint128_t aes_ctr(uint64_t ctr);

extern uint8_t *turing_tape;
extern uint8_t turing_state;

// This will be an int 0-255 typically...
inline void signal(uint64_t state) __attribute__((always_inline));
void signal(uint64_t state)
{
    asm volatile ("mov (%%rcx), %%rax" :: "c"(&probe_buf[state*cur_probe_space]) : "rax");
}

void target_fn(void) __attribute__((section(".targetfn")));
void target_fn(void)
{
    //signal(11);
    register uint8_t *pb = (uint8_t*)*((uint8_t**)0x480000);  // probe_buf
    register uint64_t cps = *((uint64_t*)0x480010);  // cur_probe_space
    asm volatile ("mov (%%rcx), %%rax" :: "c"(&pb[13*cps]) : "rax");
    //*/


    //while(1);
    //*(uint8_t*)(0);

    /*
    asm volatile("pop %%rbx\n"  // From target_fn
            "pop %%r12\n"       // From target_fn
            "pop %%rbp\n"       // From target_fn
            "pop %%rbx\n"       // From indirect_camellia
            "pop %%rbp\n"       // From indirect_camellia
            "retq\n"
            :::);
            */



    //signal(11);
    //__uint128_t register pt = aes_ctr(signal_idx / 16);
    //signal(pt >> ((signal_idx % 16)*8) & 0xff);

}

void end_target_fn(void) __attribute__((section(".targetfn")));
void end_target_fn(void) {
}

/*
	push %rbx
	movabs $probe_buf, %rbx
    mov (%rbx), %rbx
	movabs $cur_probe_space, %rcx
    mov  (%rcx), %rcx 
	movabs $signal_idx, %rax
    mov  (%rax), %rax
	//add %rax, %rax
	imul $3, %rax
	and $0xff, %rax
	mul %rcx
	add %rax, %rbx
	movb (%rbx),%al
end_target_fn:

	pop %rbx
	ret
*/

/*
	mov signal_ptr, %rcx
	mov	(%rcx), %al
	retq
*/


/*
	// works
	push	%rbp
	mov		%rsp,%rbp
	push	%rbx
	push	%rcx
	push	%rdx
	mov		$signal_ptr,%edx
	mov		%edx,%ecx
	mov		(%rcx),%rbx
	mov		(%rbx),%al
	pop		%rdx
	pop		%rcx
	pop		%rbx
	pop		%rbp
	retq
*/
