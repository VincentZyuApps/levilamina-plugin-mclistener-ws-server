#pragma once
#include <string>
#include "mod/Config.h"

namespace mclistener_ws_server {

class WebSocketServer;
class MclistenerWsServerMod;

void handleWsMessage(
    const std::string&      message,
    const Config&           config,
    WebSocketServer*        ws,
    MclistenerWsServerMod*  mod
);

} // namespace mclistener_ws_server
