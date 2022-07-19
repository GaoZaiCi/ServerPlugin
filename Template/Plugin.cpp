#include "pch.h"
#include <EventAPI.h>
#include <LoggerAPI.h>
#include <MC/Level.hpp>
#include <MC/BlockInstance.hpp>
#include <MC/Block.hpp>
#include <MC/BlockSource.hpp>
#include <MC/Actor.hpp>
#include <MC/Player.hpp>
#include <MC/ItemStack.hpp>
#include <MC/ItemInstance.hpp>
#include <MC/Item.hpp>
#include <MC/FishingHook.hpp>
#include <MC/GameMode.hpp>
#include <MC/TextPacket.hpp>
#include <MC/LoopbackPacketSender.hpp>
#include <MC/BlockLegacy.hpp>
#include <MC/Spawner.hpp>
#include <MC/Random.hpp>
#include <MC/HashedString.hpp>
#include <MC/ActorDefinitionIdentifier.hpp>
#include <MC/CommandUtils.hpp>
#include <MC/ActorUniqueID.hpp>
#include <MC/SharedAttributes.hpp>
#include <MC/AttributeInstance.hpp>
#include <MC/Explosion.hpp>
#include <MC/DispenserBlock.hpp>
#include "Version.h"
#include "PluginCommand.h"
#include "ScheduleAPI.h"
#include "Global.h"
#include "MC/VanillaItemNames.hpp"
#include <LLAPI.h>
#include <ServerAPI.h>

using namespace std;

Logger logger(PLUGIN_NAME);

inline void CheckProtocolVersion() {
    logger.info("插件开始加载");
}

void PluginInit() {
    CheckProtocolVersion();
    /*Event::PlayerJoinEvent::subscribe_ref([](auto &event) {
        event.mPlayer->sendText("欢迎玩家" + event.mPlayer->getName() + "进入游戏！");
        return true;
    });*/
    Event::RegCmdEvent::subscribe_ref([](auto &event) {
        PluginCommand::setup(event.mCommandRegistry);
        return true;
    });

}

TInstanceHook(void, "?explode@Explosion@@QEAAXXZ",
              Explosion) {
    if (PluginCommand::state) {
        setBreaking(false);
    }
    return original(this);
}

extern class HashedString EntityCanonicalName(enum ActorType);

extern enum ActorType EntityTypeFromString(std::string const &);

TInstanceHook(bool, "?_serverHooked@FishingHook@@IEAA_NXZ",
              FishingHook) {
    bool hooked = original(this);
    int tick = *(int *) ((uintptr_t) this + 1808);
    if (hooked && tick == 0) {
        Actor *actor = getOwner();
        if (actor && actor->isPlayer()) {
            auto player = (Player *) actor;
            TextPacket packet = TextPacket::createJukeboxPopup("自动钓鱼成功", {});
            ((LoopbackPacketSender *) getLevel().getPacketSender())->sendToClient(*player->getNetworkIdentifier(), packet, 0);
            ItemStack itemStack = player->getSelectedItem();
            unique_ptr<GameMode> &mode = *(unique_ptr<GameMode> *) ((uintptr_t) player + 5448);
            mode->baseUseItem(itemStack);
            player->refreshInventory();
            Schedule::delay([player, &mode]() {
                ItemStack itemStack = player->getSelectedItem();
                if (itemStack.isInstance(VanillaItemNames::FishingRod, false)) {
                    mode->baseUseItem(itemStack);
                }
            }, 3);
            auto &random = getRandom();
            int luck = random.nextInt(1, 100);
            if (luck / 2 < 10) {
                Vec3 pos = getPos();
                ActorUniqueID uniqueId = getLevel().getNewUniqueID();
                Actor *entity = CommandUtils::spawnEntityAt(player->getRegion(), pos, "minecraft:xp_bottle", uniqueId, nullptr);
                entity->lerpMotion(Vec3(0, 3, 0));
            }
            if (luck / 2 < 1) {
                Vec3 pos = getPos();
                ActorUniqueID uniqueId = getLevel().getNewUniqueID();
                Actor *entity = CommandUtils::spawnEntityAt(player->getRegion(), pos, "minecraft:creeper", uniqueId, nullptr);
                entity->setTarget(player);
                entity->setNameTag("§eBoss§r");
                AttributeInstance const &instance = entity->getAttribute(SharedAttributes::HEALTH);
                auto &p = (AttributeInstance &) instance;
                p.setMaxValue(50);
                p.setCurrentValue(50);
                entity->lerpMotion(Vec3(0, 3, 0));
            }
        } else {
            logger.error("没有找到钓鱼的玩家 {}", getUniqueID());
        }
    }
    return hooked;
}
