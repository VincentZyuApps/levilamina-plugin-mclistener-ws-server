#include "mod/ExecCommandNative.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

#include "mc/server/commands/MinecraftCommands.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandOutputMessage.h"
#include "mc/server/commands/ServerCommandOrigin.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/ServerLevel.h"
#include "mc/world/level/dimension/VanillaDimensions.h"

namespace mclistener_ws_server {

static ::MinecraftCommands*     s_cmdInstance  = nullptr;
static thread_local std::string s_captureOutput;
static thread_local bool        s_capturing    = false;

static std::string formatCmdMsg(const ::CommandOutputMessage& msg) {
    std::string s = msg.mMessageId;
    for (size_t i = 0; i < msg.mParams.size(); ++i) {
        std::string ph = "%" + std::to_string(i + 1);
        size_t pos = 0;
        while ((pos = s.find(ph, pos)) != std::string::npos) {
            s.replace(pos, ph.length(), msg.mParams[i]);
            pos += msg.mParams[i].length();
        }
    }
    return s;
}

LL_TYPE_INSTANCE_HOOK(
    CaptureOutputHook,
    ll::memory::HookPriority::Normal,
    ::MinecraftCommands,
    &::MinecraftCommands::handleOutput,
    void,
    ::CommandOrigin const& origin,
    ::CommandOutput const& output
) {
    s_cmdInstance = this;
    if (s_capturing) {
        for (auto const& msg : output.mMessages) {
            if (!s_captureOutput.empty()) s_captureOutput += '\n';
            s_captureOutput += formatCmdMsg(msg);
        }
    }
    origin(origin, output);
}

static ll::memory::HookRegistrar<CaptureOutputHook> captureOutputHookReg;

std::pair<bool, std::string> execCommandCppNative(const std::string& cmd) {
    auto lv = ll::service::getLevel();
    if (!s_cmdInstance || !lv) return {false, "server not ready"};
    auto& serverLevel = static_cast<::ServerLevel&>(*lv);
    s_capturing = true;
    s_captureOutput.clear();
    s_cmdInstance->requestCommandExecution(
        std::make_unique<::ServerCommandOrigin>(
            "ws-native", serverLevel, ::CommandPermissionLevel::Owner, ::VanillaDimensions::Overworld()),
        cmd, 29, false
    );
    std::string out = s_captureOutput;
    s_capturing = false;
    return {true, out};
}

} // namespace mclistener_ws_server
