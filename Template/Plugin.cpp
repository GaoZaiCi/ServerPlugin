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
#include <MC/ActorDamageSource.hpp>
#include <MC/ServerPlayer.hpp>
#include <MC/Container.hpp>
#include "Version.h"
#include "PluginCommand.h"
#include "ScheduleAPI.h"
#include "Global.h"
#include "MC/VanillaItemNames.hpp"
#include <LLAPI.h>
#include <ServerAPI.h>

using namespace std;

Logger logger(PLUGIN_NAME);


extern class HashedString EntityCanonicalName(enum ActorType);

extern enum ActorType EntityTypeFromString(std::string const &);

inline void CheckProtocolVersion() {
    logger.info("插件开始加载");
}

void sendText(Level &level, string const &text) {
    TextPacket packet = TextPacket::createSystemMessage(text);
    ((LoopbackPacketSender *) level.getPacketSender())->sendBroadcast(packet);
}

void PluginInit() {
    CheckProtocolVersion();
    Event::PlayerJoinEvent::subscribe_ref([](auto &event) {
        sendText(event.mPlayer->getLevel(), "§b欢迎玩家" + event.mPlayer->getName() + "进入游戏！");
        return true;
    });
    Event::RegCmdEvent::subscribe_ref([](auto &event) {
        PluginCommand::setup(event.mCommandRegistry);
        return true;
    });
    Event::MobDieEvent::subscribe_ref([](auto &event) {
        if (event.mMob->hasTag("BossCreeper")) {
            auto &level = event.mMob->getLevel();
            Actor *actor = event.mDamageSource->getEntity();
            if (actor && actor->isPlayer()) {
                sendText(event.mMob->getLevel(), "§e玩家" + CommandUtils::getActorName(*actor) + "击杀了Boss生物");
                ItemStack itemStack("minecraft:golden_apple", 1, 0, nullptr);
                itemStack.setCustomName("§6奖励物品");
                itemStack.setCustomLore({"击败Boss奖励物品"});
                auto *player = (ServerPlayer *) actor;
                player->giveItem(&itemStack);
            }
        }
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
                _pullCloser(*entity,0.1);
            }
            if (luck / 2 < 1) {
                Vec3 pos = getPos();
                {
                    ActorUniqueID uniqueId = getLevel().getNewUniqueID();
                    CommandUtils::spawnEntityAt(player->getRegion(), pos, "minecraft:lightning_bolt", uniqueId, nullptr);
                }
                ActorUniqueID uniqueId = getLevel().getNewUniqueID();
                Actor *entity = CommandUtils::spawnEntityAt(player->getRegion(), pos, "minecraft:creeper", uniqueId, nullptr);
                entity->setTarget(player);
                entity->setNameTag("§eBoss§r");
                entity->addTag("BossCreeper");
                entity->addDefinitionGroup("minecraft:charged_creeper");
                {
                    auto const &instance = entity->getAttribute(SharedAttributes::HEALTH);
                    auto &p = (AttributeInstance &) instance;
                    p.setMaxValue(50);
                    p.setCurrentValue(50);
                }
                {
                    auto const &instance = entity->getAttribute(SharedAttributes::MOVEMENT_SPEED);
                    auto &p = (AttributeInstance &) instance;
                    p.setMaxValue(50);
                    p.setCurrentValue(50);
                }
                _pullCloser(*entity,0.2);
            }
        } else {
            logger.error("没有找到钓鱼的玩家 {}", getUniqueID());
        }
    }
    return hooked;
}
