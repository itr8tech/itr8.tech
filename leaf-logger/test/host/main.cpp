#include "harness.h"

void test_isotp();
void test_decode();

int main() {
  test_isotp();
  test_decode();
  return harness_report();
}
