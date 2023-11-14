//
// Created by ASUS on 2022/2/28.
//

#ifndef MINECRAFTSUPPORT_PLUGINCOMMAND_H
#define MINECRAFTSUPPORT_PLUGINCOMMAND_H

#include <MC/Command.hpp>
#include <MC/CommandOrigin.hpp>
#include <MC/CommandOutput.hpp>
#include <MC/CommandPosition.hpp>
#include <MC/CommandRegistry.hpp>
#include "MC/CommandParameterData.hpp"

class PluginCommand : public Command {
public:
    static bool state;
public:
    static void setup(CommandRegistry *registry);

    void execute(const class CommandOrigin &, class CommandOutput &) const override;

};


#endif //MINECRAFTSUPPORT_PLUGINCOMMAND_H
