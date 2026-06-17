#include "mod/ExecCommandNative.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

#include "mc/server/commands/MinecraftCommands.h"
#include "mc/deps/core/utility/MCRESULT.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandOutputMessage.h"
#include "mc/server/commands/ServerCommandOrigin.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/CommandContext.h"
#include "mc/server/ServerLevel.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "mc/world/Minecraft.h"

namespace mclistener_ws_server {

static thread_local std::string s_captureOutput;
static thread_local bool        s_capturing = false;

static std::string translateCmdMsg(const ::CommandOutputMessage& msg) {
    static std::shared_ptr<const ::Localization> loc =
        ::getI18n().getLocaleFor(::getI18n().getCurrentLanguage()->mCode);
    return ::getI18n().get(msg.mMessageId, msg.mParams, loc);
}

LL_TYPE_INSTANCE_HOOK(
    CaptureOutputHook,
    ll::memory::HookPriority::Normal,
    ::MinecraftCommands,
    &::MinecraftCommands::handleOutput,
    void,
    ::CommandOrigin const& cmdOrigin,
    ::CommandOutput const& output
) {
    if (s_capturing) {
        for (auto const& msg : output.mMessages) {
            if (!s_captureOutput.empty()) s_captureOutput += '\n';
            s_captureOutput += translateCmdMsg(msg);
        }
    }
    origin(cmdOrigin, output);
}

static ll::memory::HookRegistrar<CaptureOutputHook> captureOutputHookReg;

std::pair<bool, std::string> execCommandCppNative(const std::string& cmd) {
    auto mc = ll::service::getMinecraft();
    auto lv = ll::service::getLevel();
    if (!mc || !mc->mCommands || !lv) return {false, "server not ready"};

    auto& serverLevel = static_cast<::ServerLevel&>(*lv);
    s_capturing = true;
    s_captureOutput.clear();

    ::CommandContext ctx;
    ctx.mCommand = cmd;
    ctx.mOrigin  = std::make_unique<::ServerCommandOrigin>(
        "ws-native", serverLevel, ::CommandPermissionLevel::Owner, ::VanillaDimensions::Overworld());
    ctx.mVersion = 29;

    mc->mCommands->requestCommandExecution(ctx, false);

    std::string out = s_captureOutput;
    s_capturing = false;
    return {true, out};
}

} // namespace mclistener_ws_server
