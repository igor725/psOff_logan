#include "libp7d/p7da.h"

#include <chrono>
#include <cstdio>
#include <thread>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <p7d file path>", argv[0]);
    return 1;
  }

  auto an = createFileAnalyser(argv[1]);
  try {
    an->run();
    printf("%s", an->spit().c_str());
  } catch (std::exception const& ex) {
    fprintf(stderr, "P7Dump exception: %s\n", ex.what());
  }

  while (true)
    std::this_thread::sleep_for(std::chrono::seconds(1));

  return 0;
}
