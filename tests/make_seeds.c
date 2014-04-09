#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifdef MINGW

static inline long int random(void) {
  return rand();
}

#endif


#define N 10000

static uint32_t random_u32(void) {
  uint32_t a, b;

  a = random();
  b = random();
  return ((a << 4) & (uint32_t) 0xFFFF0000) | (b & (uint32_t) 0xFFFF);
}

int main() {
  FILE *f;
  int i;

  f = fopen("seeds", "w");
  if (f == NULL) {
    perror("seeds");
    exit(1);
  }

  for (i=0; i<N; i++) {
    fprintf(f, "%08x\n", random_u32());
  }
  fclose(f);
  return 0;
}
