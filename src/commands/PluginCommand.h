//
// Created by ASUS on 2022/2/28.
//

#ifndef MINECRAFTSUPPORT_PLUGINCOMMAND_H
#define MINECRAFTSUPPORT_PLUGINCOMMAND_H

#include "llapi/mc/Command.hpp"
#include "llapi/mc/CommandOrigin.hpp"
#include "llapi/mc/CommandOutput.hpp"
#include "llapi/mc/CommandPosition.hpp"
#include "llapi/mc/CommandRegistry.hpp"
#include "llapi/mc/CommandParameterData.hpp"

class PluginCommand : public Command {
public:
    static bool state;
public:
    static void setup(CommandRegistry *registry);

    void execute(const class CommandOrigin &, class CommandOutput &) const override;

};


#endif //MINECRAFTSUPPORT_PLUGINCOMMAND_H
