#include "p7da.h"

#include <cstdio>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <p7d file path>", argv[0]);
    return 1;
  }

  P7DumpAnalyser an(argv[1]);
  an.run();

  return 0;
}
