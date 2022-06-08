#pragma once
#include <cstdio>
#include <iostream>
#include <errno.h>
#include <cstring>
#define ASSERT_CHECK(val, expect, msg) \
{\
  if ((val) != (expect)) \
  {\
    std::printf("[ASSERTION FAILED] at %s, %d\n", __FILE__, __LINE__);\
    std::cerr << "[Error Msg]: " << msg << \
                 "\n[EXPECT]: " << expect << "\n[GOT]: " << val << \
                 "\n[Errno]: " << std::strerror(errno) << std::endl;\
    exit(1); \
  }\
}
