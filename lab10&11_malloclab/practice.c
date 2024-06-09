#include<stdio.h>
#include<stdlib.h>

int main() {
  void *f1 = malloc(16);
  printf("0x%x\n", f1);
  void *f2 = malloc(32);
  printf("0x%x\n", f2);
  void *f3 = malloc(16);
  printf("0x%x\n", f3);
  void *f4 = malloc(32);
  printf("0x%x\n", f4);
  printf("Stage 1\n");

  free(f1);
  free(f3);

  f1 = malloc(32);
  printf("0x%x\n", f1);

  free(f2);
  free(f4); 

  return 0;

}
