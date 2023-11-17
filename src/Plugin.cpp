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
#include <mc/ActorUniqueID.hpp>
#include <MC/SharedAttributes.hpp>
#include <MC/AttributeInstance.hpp>
#include <MC/Explosion.hpp>
#include <MC/DispenserBlock.hpp>
#include <mc/ActorDamageSource.hpp>
#include <MC/ServerPlayer.hpp>
#include <MC/Container.hpp>
#include <MC/Scoreboard.hpp>
#include <mc/MobEffectInstance.hpp>
#include <MC/ActorFactory.hpp>
#include "Version.h"
#include "PluginCommand.h"
#include "ScheduleAPI.h"
#include "Global.h"
#include "GlobalServiceAPI.h"
#include "MC/VanillaItemNames.hpp"
#include "MC/EnchantUtils.hpp"
#include "MC/ListTag.hpp"
#include "MC/Dimension.hpp"
#include <LLAPI.h>
#include <ServerAPI.h>
#include "MC/Brightness.hpp"
#include "MC/ServerLevel.hpp"
#include "MC/ComponentItem.hpp"
#include "MC/AddActorPacket.hpp"
#include "mc/LeafBlock.hpp"
#include "mc/LogBlock.hpp"
#include "mc/ExperienceOrb.hpp"
#include "mc/LevelUtils.hpp"
#include "mc/Skeleton.hpp"
#include "mc/FarmBlock.hpp"
#include "mc/SharedConstants.hpp"

#include "mc/NetworkSystem.hpp"
#include "mc/LevelSettings.hpp"
#include "mc/LevelSeed64.hpp"
#include "mc/PlayerListPacket.hpp"
#include "mc/UpdateAbilitiesPacket.hpp"
#include "mc/ContainerOpenPacket.hpp"
#include "mc/ContainerClosePacket.hpp"
#include "mc/ItemStackResponsePacket.hpp"
#include "mc/InventorySlotPacket.hpp"
#include "mc/NetworkItemStackDescriptor.hpp"
#include "mc/UpdateBlockPacket.hpp"
#include "mc/BlockTypeRegistry.hpp"
#include "mc/VanillaBlockTypeIds.hpp"
#include "MC/LoopbackPacketSender.hpp"
#include "MC/ChestBlockActor.hpp"

#include "Utils.h"
#include "PocketInventory.h"
#include "BloodMoon.h"
#include "SuperMob.h"

using namespace std;
using namespace Event;

#define KILL_MOB_COUNT "kill_mob_count"
#define KILL_BOSS_COUNT "kill_boss_count"
#define BLOCK_DESTROY_COUNT "block_destroy_count"
#define BUILD_DESTROY_COUNT "block_build_count"
#define PLAYER_DIE_COUNT "player_die_count"
#define PLAYER_EAT_COUNT "player_eat_count"
#define PLAYER_ATTACK_COUNT "player_attack_count"

Logger logger(PLUGIN_NAME);

class HashedString const &EntityCanonicalName(enum ActorType);

extern enum ActorType EntityTypeFromString(std::string const &);

uintptr_t imageBaseAddr;


#define CHECK_SCORE(name) \
try {\
int v = Scoreboard::getScore(name, event.mPlayer); \
Scoreboard::setScore(name, event.mPlayer, v);\
} catch (...) {           \
Scoreboard::setScore(name, event.mPlayer, 0);\
}\

BloodMoon mBloodMoon;
SuperMob mSuperMob;
PocketInventory mPocketInventory;

void PluginInit() {
    HMODULE handle = GetModuleHandle(nullptr);
    logger.info("Build {} {}", __DATE__, __TIME__);
    logger.info("服务器增强插件开始加载 BDS句柄:{}", (void *) handle);
    imageBaseAddr = (uintptr_t) handle;

    mBloodMoon.init();
    mSuperMob.init();
    mPocketInventory.init();

    PlayerJoinEvent::subscribe_ref([](auto &event) {
        event.mPlayer->sendText("§b欢迎玩家§e" + event.mPlayer->getName() + "§b进入游戏！");
        Schedule::delay([event] {
            CHECK_SCORE(KILL_MOB_COUNT)
            CHECK_SCORE(KILL_BOSS_COUNT)
            CHECK_SCORE(BLOCK_DESTROY_COUNT)
            CHECK_SCORE(BUILD_DESTROY_COUNT)
            CHECK_SCORE(PLAYER_DIE_COUNT)
            CHECK_SCORE(PLAYER_EAT_COUNT)
            CHECK_SCORE(PLAYER_ATTACK_COUNT)
        }, 3);
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
        Schedule::delayRepeat([] {
            static char colors[] = "0123456789abcdefg";
            static char i = 0;
            std::string color(1, colors[i++]);
            ll::setServerMotd("§" + color + "BDS");
            if (i >= sizeof colors) {
                i = 0;
            }
        }, 20, 20 * 6);
        return true;
    });
    Event::MobDieEvent::subscribe_ref([](auto &event) {
        Actor *actor = event.mDamageSource->getEntity();
        if (actor && actor->isPlayer()) {
            auto *player = (ServerPlayer *) actor;
            Scoreboard::addScore(player, KILL_MOB_COUNT, 1);
            if (event.mMob->hasTag("BossCreeper")) {
                Scoreboard::addScore(player, KILL_BOSS_COUNT, 1);
                ItemStack itemStack("minecraft:golden_apple", 1, 0, nullptr);
                itemStack.setCustomName("§6奖励物品");
                itemStack.setCustomLore({"击败Boss奖励物品"});
                itemStack.getUserData()->putBoolean("BossItem", true);
                EnchantUtils::applyEnchant(itemStack, Enchant::Type::knockback, 5, false);
                player->sendText("§e玩家§b" + CommandUtils::getActorName(*actor) + "§e击杀了Boss生物");
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

}

TInstanceHook(void, "?_subTick@ServerLevel@@MEAAXXZ", ServerLevel) {
    mBloodMoon.onTick(this);
    return original(this);
}

// Spawner::spawnItem(BlockSource &,ItemStack const &,Actor *,Vec3 const &,int)
TInstanceHook(ItemActor*, "?spawnItem@Spawner@@QEAAPEAVItemActor@@AEAVBlockSource@@AEBVItemStack@@PEAVActor@@AEBVVec3@@H@Z", Spawner, BlockSource &source, ItemStack const &itemStack, Actor *actor, Vec3 const &pos, int value) {
    if (actor && actor->getActorIdentifier().getCanonicalName() == "minecraft:fishing_hook") {
        auto *fishingHook = (FishingHook *) actor;
        auto *player = (Player *) fishingHook->getOwner();
        if (player && player->isPlayer()) {
            Random &random = player->getRandom();
            if (random.nextInt(0, 100) > 70) {
                ExperienceOrb::spawnOrbs(source, player->getPos(), 5, (ExperienceOrb::DropType) 2, nullptr);
            }
            if (random.nextInt(0, 100) > 98) {
                ItemStack newItemStack(itemStack);
                Utils::enchant(newItemStack, EnchantType::durability, MINSHORT);
                return original(this, source, newItemStack, actor, pos, value);
            }
            if (itemStack.getRawNameId() == "enchanted_book") {
                if (random.nextInt(0, 100) > 95) {
                    ItemStack newItemStack(itemStack);
                    if (random.nextBoolean()) {
                        Utils::enchant(newItemStack, EnchantType::durability, 10, true);
                    }
                    if (random.nextBoolean()) {
                        Utils::enchant(newItemStack, EnchantType::digging, 10, true);
                    }
                    if (random.nextBoolean()) {
                        Utils::enchant(newItemStack, EnchantType::protect_all, 10, true);
                    }
                    if (random.nextBoolean()) {
                        Utils::enchant(newItemStack, EnchantType::tridentRiptide, 10, true);
                    }
                    if (random.nextInt(100) > 70) {
                        Utils::enchant(newItemStack, EnchantType::damage_all, 10, true);
                    }
                    if (random.nextInt(100) > 70) {
                        Utils::enchant(newItemStack, EnchantType::lootBonusDigger, 10, true);
                    }
                    return original(this, source, newItemStack, actor, pos, value);
                }
            }
            if (itemStack.getRawNameId() == "fishing_rod") {
                if (random.nextInt(0, 100) > 95) {
                    ItemStack newItemStack(itemStack);
                    if (random.nextBoolean()) {
                        Utils::enchant(newItemStack, EnchantType::durability, 10);
                    }
                    if (random.nextBoolean()) {
                        Utils::enchant(newItemStack, EnchantType::mending, 10);
                    }
                    if (random.nextBoolean()) {
                        Utils::enchant(newItemStack, EnchantType::knockback, 10);
                    }
                    if (random.nextBoolean()) {
                        Utils::enchant(newItemStack, EnchantType::lootBonusFishing, 10);
                    } else {
                        Utils::enchant(newItemStack, EnchantType::fishingSpeed, 10);
                    }
                    return original(this, source, newItemStack, actor, pos, value);
                }
            }
        }
    }
    return original(this, source, itemStack, actor, pos, value);
}

// ItemActor::_merge(ItemActor *)
TInstanceHook(bool, "?_merge@ItemActor@@AEAA_NPEAV1@@Z", ItemActor, ItemActor *actor) {
    auto *item1 = (ItemStack *) ((uintptr_t) this + 1184);
    auto *item2 = (ItemStack *) ((uintptr_t) actor + 1184);
    if (item1 && item2 && this != actor) {
        if (item1->getRawNameId() == "golden_apple" && item2->getRawNameId() == "gold_block") {
            if (item2->getCount() >= 8) {
                ItemStack itemStack("minecraft:enchanted_golden_apple", 1, 0, nullptr);
                getLevel().getSpawner().spawnItem(getRegion(), itemStack, nullptr, getPos(), 0);
                if (item1->getCount() > 1) {
                    item1->set(item1->getCount() - 1);
                } else {
                    remove();
                }
                if (item2->getCount() > 8) {
                    item2->set(item2->getCount() - 8);
                } else if (item2->getCount() == 8) {
                    actor->remove();
                }
            }
        }
    }
    return original(this, actor);
}

// FarmBlock::_becomeDirt(BlockSource &,BlockPos const &,Actor *)
TInstanceHook(void, "?_becomeDirt@FarmBlock@@AEBAXAEAVBlockSource@@AEBVBlockPos@@PEAVActor@@@Z", FarmBlock, BlockSource &source, BlockPos const &pos, Actor *actor) {
}


// UpdateSubChunkBlocksPacket::UpdateSubChunkBlocksPacket(
// std::vector<UpdateSubChunkBlocksPacket::NetworkBlockInfo> const &,
// std::vector<UpdateSubChunkBlocksPacket::NetworkBlockInfo> const &)
class UpdateSubChunkBlocksPacket : public Packet {
public:
};


TInstanceHook(void, "?send@NetworkSystem@@QEAAXAEBVNetworkIdentifier@@AEBVPacket@@W4SubClientId@@@Z", NetworkSystem, NetworkIdentifier const &identifier, Packet const &packet, SubClientId id) {
    /*if (packet.getId() != MinecraftPacketIds::MoveActorDelta && packet.getId() != MinecraftPacketIds::SetActorData && packet.getId() != MinecraftPacketIds::SetActorMotion) {
        logger.info("Sending {}", packet.getName());
    }*/
    /*if (packet.getId() == MinecraftPacketIds::PlayerList) {
        auto ptr = (PlayerListPacket *) &packet;
        auto players = Level::getAllPlayers();
        for (auto &it: players) {
            if (identifier == *it->getNetworkIdentifier() && it->isOperator()) {
                return original(this, identifier, packet, id);
            }
        }
        for (auto &it: ptr->entries) {
            it.mIsHost = false;
        }
    }
    if (packet.getId() == MinecraftPacketIds::UpdateAbilities) {
        auto ptr = (UpdateAbilitiesPacket *) &packet;
        auto players = Level::getAllPlayers();
        for (auto &it: players) {
            if (identifier == *it->getNetworkIdentifier() && it->isOperator()) {
                return original(this, identifier, packet, id);
            }
        }
        ptr->mData.mPlayerPermissionLevel = PlayerPermissionLevel::Member;
    }
    if (packet.getId() == MinecraftPacketIds::ContainerOpen) {
        auto ptr = (ContainerOpenPacket *) &packet;
        // ServerPlayer::_nextContainerCounter
        ContainerID containerId = dAccess<ContainerID, 48>(ptr);
        ContainerType containerType = dAccess<ContainerType, 49>(ptr);
        BlockPos pos = dAccess<BlockPos, 52>(ptr);
        ActorUniqueID uniqueId = dAccess<ActorUniqueID, 64>(ptr);
        logger.info("ContainerOpen {} {} {} {}", (int) containerId, (int) containerType, pos.toString(), uniqueId);
        // 2 0 (1619, 110, 50) -1
    }
    if (packet.getId() == MinecraftPacketIds::ContainerClose) {
        auto &ptr = (ContainerClosePacket &) packet;
        mPocketInventory.onContainerClosePacket(ptr);
    }
    if (packet.getId() == MinecraftPacketIds::ItemStackResponse) {
        auto ptr = (ItemStackResponsePacket *) &packet;
        auto &list = dAccess<vector<ItemStackResponseInfo>, 48>(ptr);
        logger.info("ItemStackResponse {}", list.size());
        for (auto &it: list) {
            logger.info("ItemStackResponse {}", it.toString());
        }
    }
    if (packet.getId() == MinecraftPacketIds::InventorySlot) {
        auto ptr = (InventorySlotPacket *) &packet;
        ContainerID containerId = dAccess<ContainerID, 48>(ptr);
        uint32_t slot = dAccess<uint32_t, 52>(ptr);
        auto &item = dAccess<NetworkItemStackDescriptor, 56>(ptr);

        BlockPalette &blockPalette = Global<ServerLevel>->getBlockPalette();
        ItemStack itemStack = ItemStack::fromDescriptor(item, blockPalette, true);
        logger.info("InventorySlot {} {} {}", (int) containerId, slot, itemStack.toDebugString());
    }
    if (packet.getId() == MinecraftPacketIds::UpdateBlock) {
        auto ptr = (UpdateBlockPacket *) &packet;
        BlockPos pos = dAccess<BlockPos, 48>(ptr);
        uint32_t id1 = dAccess<uint32_t, 60>(ptr);
        uint8_t data = dAccess<uint8_t, 64>(ptr);
        uint32_t mRuntimeId = dAccess<uint32_t, 68>(ptr);
        logger.info("UpdateBlock {} {} {} {}", pos.toString(), id1, (int) data, mRuntimeId);
    }*/
    return original(this, identifier, packet, id);
}


TInstanceHook(void, "?spawnAtLocation@Actor@@QEAAPEAVItemActor@@AEBVItemStack@@M@Z", Actor, ItemStack const &itemStack, float value) {
    if (hasTag("BloodMoon") || hasTag("SuperMob")) {
        auto &item = (ItemStack &) itemStack;
        item.add(item.getCount());
    }
    return original(this, itemStack, value);
}

/*TInstanceHook(void, "?tick@Spawner@@QEAAXAEAVBlockSource@@AEBVLevelChunk@@@Z", Spawner, BlockSource &source, LevelChunk const &chunk) {
    //Level *level = dAccess<Level *, 0>(this);
    return original(this, source, chunk);
}*/

TInstanceHook(void, "?releaseUsingItem@GameMode@@UEAAXXZ", GameMode) {
    ItemStack const &itemStack = getPlayer()->getSelectedItem();
    if (!itemStack.isNull() && itemStack.getItem()->isFood() && (getPlayer()->isHungry() || getPlayer()->forceAllowEating())) {
        Scoreboard::addScore(getPlayer(), PLAYER_EAT_COUNT, 1);
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
TInstanceHook(void, "?explode@Explosion@@QEAAXXZ", Explosion) {
    if (PluginCommand::state) {
        ActorUniqueID id = dAccess<ActorUniqueID, 88>(this);
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


TInstanceHook(bool, "?_serverHooked@FishingHook@@IEAA_NXZ", FishingHook) {
    bool hooked = original(this);
    int tick = dAccess<int, 1224>(this);
    if (hooked && tick == 0) {
        Actor *actor = getOwner();
        if (actor && actor->isPlayer()) {
            auto player = (Player *) actor;
            player->sendText("自动钓鱼成功", TextType::JUKEBOX_POPUP);
            ItemStack itemStack = player->getSelectedItem();
            auto &mode = dAccess<unique_ptr<GameMode>, 3696>(player);
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
                entity->addTag("SuperMob");
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
            logger.error("没有找到钓鱼的玩家 {}", getOrCreateUniqueID());
        }
    }
    return hooked;
}
