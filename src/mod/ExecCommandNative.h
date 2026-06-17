#pragma once
#include <string>
#include <utility>

namespace mclistener_ws_server {

// Execute a BDS command natively and capture its output.
// Returns {success, output_text}.
std::pair<bool, std::string> execCommandCppNative(const std::string& cmd);

} // namespace mclistener_ws_server
