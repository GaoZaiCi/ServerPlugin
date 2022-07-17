//
// Created by ASUS on 2022/2/28.
//

#include "PluginCommand.h"

bool PluginCommand::state = true;

void PluginCommand::setup(CommandRegistry *registry) {
    registry->registerCommand("explode", "防爆保护", CommandPermissionLevel::GameMasters, {CommandFlagValue::None}, {(CommandFlagValue) 0x80});
    registry->registerOverload<PluginCommand>("explode");
}

void PluginCommand::execute(const CommandOrigin &origin, CommandOutput &output) const {
    state = !state;
    output.success(state ? "§e已开启防爆" : "§c已关闭防爆");
}
