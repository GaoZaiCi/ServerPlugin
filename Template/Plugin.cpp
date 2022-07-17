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
#include <MC/PlayerEventCoordinator.hpp>
#include <MC/ServerPlayerEventCoordinator.hpp>
#include "Version.h"
#include "PluginCommand.h"
#include "ScheduleAPI.h"
#include "Global.h"
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


class Explosion {
public:
    float x, y, value;
public:
    ActorUniqueID &getUniqueID() {
        return *(ActorUniqueID *) ((uintptr_t) this + 88);
    }
};


TInstanceHook(void, "?explode@Explosion@@QEAAXXZ",
              Explosion) {
    if (PluginCommand::state) {
        Actor *actor = Level::getEntity(getUniqueID());

        auto entities = Level::getAllEntities();
        for (auto &it: entities) {
            float dis = actor->getPos().distanceTo(it->getPos());
            if (it != actor && dis < 7) {
                it->hurtEntity(20 - dis);
            }
        }
        return;
    }
    return original(this);
}


TInstanceHook(bool, "?_serverHooked@FishingHook@@IEAA_NXZ",
              FishingHook) {
    bool hooked = original(this);
    int tick = *(int *) ((uintptr_t) this + 1808);

    if (hooked && tick == 0) {
        if (getOwnerId() != -1) {
            auto player = (Player *) getLevel().fetchEntity(getOwnerId(), false);
            if (player == nullptr) {
                logger.error("钓鱼的玩家数据错误 {}", getOwnerId());
                return hooked;
            }
            TextPacket packet = TextPacket::createJukeboxPopup("自动钓鱼成功", {});
            ((LoopbackPacketSender *) getLevel().getPacketSender())->sendToClient(*player->getNetworkIdentifier(), packet, 0);
            ItemStack itemStack = player->getSelectedItem();
            itemStack.use(*player);
            player->refreshInventory();
            Schedule::delay([player]() {
                ItemStack itemStack = player->getSelectedItem();
                itemStack.use(*player);
                player->refreshInventory();
            }, 3);
        } else {
            logger.error("没有找到钓鱼的玩家 {}", getUniqueID());
        }
    }
    return hooked;
}
