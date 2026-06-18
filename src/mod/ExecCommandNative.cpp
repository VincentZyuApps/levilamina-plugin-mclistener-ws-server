#include "mod/ExecCommandNative.h"

#include "ll/api/service/Bedrock.h"

#include "mc/server/commands/MinecraftCommands.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandOutputMessage.h"
#include "mc/server/commands/CommandOutputType.h"
#include "mc/server/commands/ServerCommandOrigin.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/CommandVersion.h"
#include "mc/server/commands/CurrentCmdVersion.h"
#include "mc/server/ServerLevel.h"
#include "mc/world/level/dimension/VanillaDimensions.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "mc/world/Minecraft.h"

namespace mclistener_ws_server {

std::pair<bool, std::string> execCommandCppNative(const std::string& cmd) {
    auto mc = ll::service::getMinecraft();
    auto lv = ll::service::getLevel();
    if (!mc || !mc->mCommands || !lv) return {false, "server not ready"};

    auto& serverLevel = static_cast<::ServerLevel&>(*lv);
    auto  origin = ::ServerCommandOrigin(
        "ws-native", serverLevel, ::CommandPermissionLevel::Owner, ::VanillaDimensions::Overworld());

    std::string compileErr;
    auto command = mc->mCommands->compileCommand(
        cmd,
        origin,
        ::CurrentCmdVersion::Latest,
        [&](std::string const& err) { compileErr.append(err).append("\n"); }
    );

    if (!command) {
        if (!compileErr.empty() && compileErr.back() == '\n') compileErr.pop_back();
        return {false, compileErr.empty() ? "command compile failed" : compileErr};
    }

    ::CommandOutput output(::CommandOutputType::AllOutput);
    command->run(origin, output);

    static std::shared_ptr<const ::Localization> loc =
        ::getI18n().getLocaleFor(::getI18n().getCurrentLanguage()->mCode);

    std::string outputStr;
    for (auto const& msg : output.mMessages) {
        if (!outputStr.empty()) outputStr += '\n';
        outputStr += ::getI18n().get(msg.mMessageId, msg.mParams, loc);
    }
    if (!outputStr.empty() && outputStr.back() == '\n') outputStr.pop_back();

    return {output.mSuccessCount > 0, outputStr};
}

} // namespace mclistener_ws_server
