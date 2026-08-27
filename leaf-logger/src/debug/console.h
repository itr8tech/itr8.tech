// Serial console for the driveway validation session.
//
// This is the tool that resolves the unknown byte offsets: capture a raw
// payload here, read the matching value off LeafSpy on your phone at the same
// moment, then feed both to tools/find_offsets on your laptop.
#pragma once

#include "../transport/lbc_client.h"
#include "../transport/twai_bus.h"

namespace debugconsole {

void PrintBanner();
void PrintHelp();

// Non-blocking: call from loop(). Returns true if a command was handled.
bool Poll(transport::TwaiBus& bus, transport::LbcClient& lbc);

}  // namespace debugconsole
