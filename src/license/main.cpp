#include <string.h>

#include <iostream>

#include <common/macros.h>

#include <common/license/gen_hardware_hash.h>

#define HELP_TEXT                              \
  "Usage: " LICENSE_GEN_NAME                   \
  " [options]\n"                               \
  "  License generation for " LICENSE_GEN_NAME \
  " project.\n\n"                              \
  "    --hdd [default] hash algorithm\n"       \
  "    --machine-id hash algorithm\n"

int main(int argc, char** argv) {
  UNUSED(argc);
  UNUSED(argv);
  common::license::ALGO_TYPE algo = common::license::HDD;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--hdd") == 0) {
      algo = common::license::HDD;
    } else if (strcmp(argv[i], "--machine-id") == 0) {
      algo = common::license::MACHINE_ID;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      std::cout << HELP_TEXT << std::endl;
      return EXIT_SUCCESS;
    } else {
      std::cout << HELP_TEXT << std::endl;
      return EXIT_SUCCESS;
    }
  }

  std::string hash;
  if (!common::license::GenerateHardwareHash(algo, &hash)) {
    return EXIT_FAILURE;
  }

  std::cout << hash << std::endl;
  return EXIT_SUCCESS;
}