#include<stdio.h>

int main(){
	int no=100;
	int val;
    asm(
        "addq $5 %1\n\t"
        : "=r" (val)
        : "r" (no)
    )
	/*asm( */
        /*"movl %1, %%ebx\n\t" */
        /*"add $5 %%ebx\n\t" */
        /*"movl %%ebx %0\n\t" */
	/*	/1* */
	/*	 * Write an instruction that moves value of no to %ebx */
	/*	 * Write an instruction that adds 5 (constant) to %ebx */
	/*	 * Write an instruction that moves value of %ebx to return */
	/*	 *1/ */			
	/*	: "=r" (val) */
	/*	: "r" (no) */
	/*	: "ebx" */
	/*); */

	printf("%d \n", val);
	return 0;
}


