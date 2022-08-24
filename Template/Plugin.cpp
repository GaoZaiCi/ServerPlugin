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
#include <MC/Scoreboard.hpp>
#include <MC/MobEffectInstance.hpp>
#include <MC/ActorFactory.hpp>
#include "Version.h"
#include "PluginCommand.h"
#include "ScheduleAPI.h"
#include "Global.h"
#include "MC/VanillaItemNames.hpp"
#include "MC/EnchantUtils.hpp"
#include <LLAPI.h>
#include <ServerAPI.h>

using namespace std;

Logger logger(PLUGIN_NAME);


class HashedString const &EntityCanonicalName(enum ActorType);

extern enum ActorType EntityTypeFromString(std::string const &);

inline void CheckProtocolVersion() {
    logger.info("插件开始加载");
}

#define KILL_MOB_COUNT "kill_mob_count"
#define KILL_BOSS_COUNT "kill_boss_count"
#define BLOCK_DESTROY_COUNT "block_destroy_count"
#define BUILD_DESTROY_COUNT "block_build_count"
#define PLAYER_DIE_COUNT "player_die_count"
#define PLAYER_EAT_COUNT "player_eat_count"
#define PLAYER_ATTACK_COUNT "player_attack_count"

void PluginInit() {
    CheckProtocolVersion();
    Event::PlayerJoinEvent::subscribe_ref([](auto &event) {
        event.mPlayer->sendText("§b欢迎玩家" + event.mPlayer->getName() + "进入游戏！");
        return true;
    });
    Event::RegCmdEvent::subscribe_ref([](auto &event) {
        PluginCommand::setup(event.mCommandRegistry);
        return true;
    });
    Event::ServerStartedEvent::subscribe_ref([](auto &event) {
        Scoreboard::newObjective(KILL_MOB_COUNT, "生物击杀数");
        Scoreboard::newObjective(KILL_BOSS_COUNT, "Boss击杀数");
        Scoreboard::newObjective(BLOCK_DESTROY_COUNT, "方块破坏数");
        Scoreboard::newObjective(BUILD_DESTROY_COUNT, "方块放置数");
        Scoreboard::newObjective(PLAYER_DIE_COUNT, "玩家死亡数");
        Scoreboard::newObjective(PLAYER_EAT_COUNT, "玩家食用数");
        Scoreboard::newObjective(PLAYER_ATTACK_COUNT, "玩家攻击数");
        return true;
    });
    Event::MobDieEvent::subscribe_ref([](auto &event) {
        Actor *actor = event.mDamageSource->getEntity();
        if (actor && actor->isPlayer()) {
            auto *player = (ServerPlayer *) actor;
            Scoreboard::addScore(player, KILL_MOB_COUNT, 1);
            if (event.mMob->hasTag("BossCreeper")) {
                Scoreboard::addScore(player, KILL_BOSS_COUNT, 1);
                auto &level = event.mMob->getLevel();
                ItemStack itemStack("minecraft:golden_apple", 1, 0, nullptr);
                itemStack.setCustomName("§6奖励物品");
                itemStack.setCustomLore({"击败Boss奖励物品"});
                itemStack.getUserData()->putBoolean("BossItem", true);
                EnchantUtils::applyEnchant(itemStack, Enchant::Type::knockback, 5, false);
                player->sendText("§e玩家" + CommandUtils::getActorName(*actor) + "击杀了Boss生物");
                player->giveItem(&itemStack);
            }
        }
        return true;
    });
    Event::PlayerDestroyBlockEvent::subscribe_ref([](auto &event) {
        Scoreboard::addScore(event.mPlayer, BLOCK_DESTROY_COUNT, 1);
        return true;
    });
    Event::PlayerPlaceBlockEvent::subscribe_ref([](auto &event) {
        Scoreboard::addScore(event.mPlayer, BUILD_DESTROY_COUNT, 1);
        return true;
    });
    Event::PlayerDieEvent::subscribe_ref([](auto &event) {
        Scoreboard::addScore(event.mPlayer, PLAYER_DIE_COUNT, 1);
        return true;
    });
    Event::PlayerAttackEvent::subscribe_ref([](auto &event) {
        Scoreboard::addScore(event.mPlayer, PLAYER_ATTACK_COUNT, 1);
        return true;
    });
    Event::MobSpawnEvent::subscribe_ref([](auto &event) {
        return true;
    });
}

TInstanceHook(Mob*, "?spawnMob@Spawner@@QEAAPEAVMob@@AEAVBlockSource@@AEBUActorDefinitionIdentifier@@PEAVActor@@AEBVVec3@@_N44@Z", Spawner, BlockSource &source, ActorDefinitionIdentifier const &identifier, Actor *entity, Vec3 const &pos, bool b1, bool b2, bool b3) {
    Mob *mob = original(this, source, identifier, entity, pos, b1, b2, b3);
    if (mob && identifier.getCanonicalName() == "minecraft:creeper") {
        if (mob->getRandom().nextInt(0,100) > 70){
            mob->setNameTag("§e闪击Boss§r");
            mob->addDefinitionGroup("minecraft:charged_creeper");
            Level *level = dAccess<Level*,0>(this);
            Player *player = level->getPlayer("VanessaSpy");
            if (player){
                //设置生物攻击目标
                mob->setTarget(player);
                if (player->distanceTo(*mob) < 50.0f){
                    auto const &instance = entity->getAttribute(SharedAttributes::HEALTH);
                    auto &p = (AttributeInstance &) instance;
                    p.setMaxValue(40);
                    p.setCurrentValue(40);
                }
            }
        }
    }
    return mob;
}

TInstanceHook(void, "?releaseUsingItem@GameMode@@UEAAXXZ",
              GameMode) {
    ItemStack const &itemStack = getPlayer()->getSelectedItem();
    if (!itemStack.isNull() && itemStack.getItem()->isFood()) {
        Scoreboard::addScore(getPlayer(), PLAYER_EAT_COUNT, 1);
        /*CompoundTag const *tag = itemStack.getUserData();
        if (tag && tag->contains("BossItem")) {
            getPlayer()->addEffect(MobEffectInstance((uint32_t) MobEffect::EffectType::InstantHealth, 20 * 60, 50));
            getPlayer()->addEffect(MobEffectInstance((uint32_t) MobEffect::EffectType::JumpBoost, 20 * 60, 50));
        }*/
    }
    return original(this);
}

/**
 * 会产生爆炸的实体
 * minecraft:wither
 * minecraft:creeper
 * minecraft:fireball
 * minecraft:tnt
 * minecraft:ender_crystal
 * minecraft:wither_skull
 * minecraft:wither_skull_dangerous
 * minecraft:respawn_anchor
 */
TInstanceHook(void, "?explode@Explosion@@QEAAXXZ",
              Explosion) {
    if (PluginCommand::state) {
        ActorUniqueID id = *(ActorUniqueID *) ((uintptr_t) this + 88);
        Actor *actor = Level::getEntity(id);
        if (actor) {
            string const &name = actor->getActorIdentifier().getCanonicalName();
            if (name == "minecraft:creeper" || name == "minecraft:fireball" || name == "minecraft:respawn_anchor") {
                setBreaking(false);
            }
        }
    }
    return original(this);
}


TInstanceHook(bool, "?_serverHooked@FishingHook@@IEAA_NXZ",
              FishingHook) {
    bool hooked = original(this);
    int tick = dAccess<int,1816>(this);
    if (hooked && tick == 0) {
        Actor *actor = getOwner();
        if (actor && actor->isPlayer()) {
            auto player = (Player *) actor;
            player->sendText("自动钓鱼成功", TextType::JUKEBOX_POPUP);
            ItemStack itemStack = player->getSelectedItem();
            unique_ptr<GameMode> &mode = dAccess<unique_ptr<GameMode>,5928>(player);
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
                _pullCloser(*entity, 0.1f);
            } else if (luck / 2 < 1) {
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
                if (random.nextInt(1, 100) / 2 < 1) {
                    {
                        auto const &instance = entity->getAttribute(SharedAttributes::HEALTH);
                        auto &p = (AttributeInstance &) instance;
                        p.setMaxValue(1000);
                        p.setCurrentValue(1000);
                    }
                } else {
                    {
                        auto const &instance = entity->getAttribute(SharedAttributes::HEALTH);
                        auto &p = (AttributeInstance &) instance;
                        p.setMaxValue(100);
                        p.setCurrentValue(100);
                    }
                }
                {
                    auto const &instance = entity->getAttribute(SharedAttributes::MOVEMENT_SPEED);
                    auto &p = (AttributeInstance &) instance;
                    p.setMaxValue(20);
                    p.setCurrentValue(20);
                }
                _pullCloser(*entity, 0.2f);
            }
        } else {
            logger.error("没有找到钓鱼的玩家 {}", getUniqueID());
        }
    }
    return hooked;
}
