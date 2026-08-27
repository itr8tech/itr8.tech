// Resolves an unknown field encoding inside a captured LBC payload.
//
// Give it a payload captured from the car and a value you can read off LeafSpy
// at the same moment, and it reports every (offset, width, signedness, scale)
// combination that produces that value. For a distinctive number — 87.5% SOH,
// 56.3 Ah — that is usually one candidate, which is your offset.
//
// This is the tedious half of the driveway session done for you.
//
//   ./build/find_offsets "61 01 00 00 03 E8 ..." --target 87.5 --tol 0.05
//
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../src/leaf/decode.h"

namespace {

std::vector<uint8_t> ParseHex(const std::string& s) {
  std::vector<uint8_t> out;
  int hi = -1;
  for (char c : s) {
    int v;
    if (c >= '0' && c <= '9') v = c - '0';
    else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
    else continue;  // Skip spaces, commas, 0x prefixes, newlines.
    if (hi < 0) hi = v;
    else { out.push_back(static_cast<uint8_t>((hi << 4) | v)); hi = -1; }
  }
  return out;
}

struct Scale {
  const char* label;
  double value;
};

// Scales seen across published Leaf decode tables: plain integers, decimal
// tenths/hundredths, and the binary fractions the LBC actually uses internally.
const Scale kScales[] = {
    {"1", 1.0},           {"1/2", 0.5},          {"1/4", 0.25},
    {"1/5", 0.2},         {"1/8", 0.125},        {"1/10", 0.1},
    {"1/16", 1.0 / 16},   {"1/32", 1.0 / 32},    {"1/50", 0.02},
    {"1/64", 1.0 / 64},   {"1/100", 0.01},       {"1/128", 1.0 / 128},
    {"1/256", 1.0 / 256}, {"1/512", 1.0 / 512},  {"1/1000", 0.001},
    {"1/1024", 1.0 / 1024},
    {"1/10000", 1e-4},    {"1/100000", 1e-5},
    {"2", 2.0},           {"4", 4.0},            {"10", 10.0},
    {"80 (gid->Wh)", 80.0}, {"77.5 (gid->Wh)", 77.5},
};

void Usage() {
  std::fprintf(stderr,
               "usage: find_offsets \"<hex payload>\" --target <value> [--tol <t>]\n"
               "                    [--max-width N] [--unsigned-only]\n\n"
               "  --target   the true value, read off LeafSpy at capture time\n"
               "  --tol      absolute tolerance (default 0.01)\n"
               "  --max-width  widest field to consider, 1-4 (default 4)\n");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { Usage(); return 2; }

  const std::vector<uint8_t> payload = ParseHex(argv[1]);
  double target = 0.0;
  double tol = 0.01;
  size_t max_width = 4;
  bool unsigned_only = false;
  bool have_target = false;

  for (int i = 2; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--target") && i + 1 < argc) {
      target = std::atof(argv[++i]); have_target = true;
    } else if (!std::strcmp(argv[i], "--tol") && i + 1 < argc) {
      tol = std::atof(argv[++i]);
    } else if (!std::strcmp(argv[i], "--max-width") && i + 1 < argc) {
      max_width = static_cast<size_t>(std::atoi(argv[++i]));
    } else if (!std::strcmp(argv[i], "--unsigned-only")) {
      unsigned_only = true;
    } else {
      Usage(); return 2;
    }
  }

  if (payload.empty()) {
    std::fprintf(stderr, "error: could not parse any bytes from the payload\n");
    return 2;
  }
  if (!have_target) { Usage(); return 2; }
  if (max_width < 1 || max_width > 4) max_width = 4;

  std::printf("payload: %zu bytes", payload.size());
  if (payload.size() >= 2 && payload[0] == 0x61)
    std::printf("  (response to group 0x%02X)", payload[1]);
  std::printf("\ntarget:  %g  (tolerance %g)\n\n", target, tol);

  int hits = 0;
  for (size_t off = 0; off < payload.size(); ++off) {
    for (size_t w = 1; w <= max_width; ++w) {
      if (off + w > payload.size()) break;

      uint32_t u = 0;
      int32_t s = 0;
      const bool have_u = leaf::ReadUnsigned(payload.data(), payload.size(), off, w, u);
      const bool have_s = leaf::ReadSigned(payload.data(), payload.size(), off, w, s);

      for (const Scale& sc : kScales) {
        if (have_u && std::fabs(u * sc.value - target) <= tol) {
          std::printf("  offset %3zu  width %zu  unsigned  raw %10u  x %-14s = %g\n",
                      off, w, u, sc.label, u * sc.value);
          ++hits;
        }
        if (!unsigned_only && have_s && s < 0 &&
            std::fabs(s * sc.value - target) <= tol) {
          std::printf("  offset %3zu  width %zu  signed    raw %10d  x %-14s = %g\n",
                      off, w, s, sc.label, s * sc.value);
          ++hits;
        }
      }
    }
  }

  if (hits == 0) {
    std::printf("  no candidates.\n\n"
                "  Things to try: widen --tol, check the value was read at the same\n"
                "  moment as the capture, or confirm whether your reference source\n"
                "  quotes offsets including the `61 xx` echo (this tool does).\n");
  } else {
    std::printf("\n%d candidate%s. A single hit on a distinctive value is almost\n"
                "certainly the real encoding; several hits means capture a second\n"
                "sample at a different value and intersect the results.\n",
                hits, hits == 1 ? "" : "s");
  }
  return 0;
}
