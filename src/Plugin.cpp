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
#include "VftableHook.h"
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

using namespace std;

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

bool BloodMoon;

#define CHECK_SCORE(name) \
try {\
int v = Scoreboard::getScore(name, event.mPlayer); \
Scoreboard::setScore(name, event.mPlayer, v);\
} catch (...) {           \
Scoreboard::setScore(name, event.mPlayer, 0);\
}\

PocketInventory mPocketInventory;

void PluginInit() {
    HMODULE handle = GetModuleHandle(nullptr);
    logger.info("Build {} {}", __DATE__, __TIME__);
    logger.info("服务器增强插件开始加载 BDS句柄:{}", (void *) handle);
    imageBaseAddr = (uintptr_t) handle;
    mPocketInventory.init();
    Event::PlayerJoinEvent::subscribe_ref([](auto &event) {
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
    Event::MobSpawnedEvent::subscribe_ref([](auto &event) {
        Mob *mob = event.mMob;
        BlockSource &source = mob->getRegion();
        ActorDefinitionIdentifier identifier = mob->getActorIdentifier();
        const string &name = identifier.getCanonicalName();
        if (name == "minecraft:creeper") {
            if (mob->getRandom().nextInt(0, 100) > 70) {
                mob->addTag("SuperMob");
                mob->setNameTag("闪电苦力怕");
                mob->addDefinitionGroup("minecraft:charged_creeper");
                Level &level = source.getLevel();
                Player *player = level.getPlayer("VanessaSpy");
                if (player) {
                    //设置生物攻击目标
                    mob->setTarget(player);
                    if (player->distanceTo(*mob) < 50.0f) {
                        auto const &instance = mob->getAttribute(SharedAttributes::HEALTH);
                        auto &p = (AttributeInstance &) instance;
                        p.setMaxValue(40);
                        p.setCurrentValue(40);
                    }
                }
            }
        } else if (name == "minecraft:zombie" || name == "minecraft:zombie_villager" || name == "minecraft:husk") {
            if (mob->getRandom().nextInt(0, 100) > 60) {
                if (mob->getRandom().nextBoolean()) {
                    mob->addTag("SuperMob");
                    mob->setNameTag("Grumm");
                    if (mob->getRandom().nextInt(0, 100) > 50) {
                        ItemStack itemStack("minecraft:diamond_sword", 1, 0, nullptr);
                        unique_ptr<ListTag> listTag = ListTag::create();
                        {
                            unique_ptr<CompoundTag> compoundTag = CompoundTag::create();
                            compoundTag->putShort("id", EnchantType::durability);
                            compoundTag->putShort("lvl", MINSHORT);
                            listTag->add(std::move(compoundTag));
                        }
                        {
                            unique_ptr<CompoundTag> compoundTag = CompoundTag::create();
                            compoundTag->putShort("id", EnchantType::damage_arthropods);
                            compoundTag->putShort("lvl", MINSHORT);
                            listTag->add(std::move(compoundTag));
                        }
                        {
                            unique_ptr<CompoundTag> compoundTag = CompoundTag::create();
                            compoundTag->putShort("id", EnchantType::damage_undead);
                            compoundTag->putShort("lvl", MINSHORT);
                            listTag->add(std::move(compoundTag));
                        }
                        {
                            unique_ptr<CompoundTag> compoundTag = CompoundTag::create();
                            compoundTag->putShort("id", EnchantType::mending);
                            compoundTag->putShort("lvl", MINSHORT);
                            listTag->add(std::move(compoundTag));
                        }
                        if (itemStack.hasUserData()) {
                            itemStack.getUserData()->put(ItemStack::TAG_ENCHANTS, std::move(listTag));
                        } else {
                            unique_ptr<CompoundTag> compoundTag = CompoundTag::create();
                            compoundTag->put(ItemStack::TAG_ENCHANTS, std::move(listTag));
                            itemStack.setUserData(std::move(compoundTag));
                        }
                        mob->setCarriedItem(itemStack);
                    }
                } else {
                    mob->setNameTag("Dinnerbone");
                    if (mob->getRandom().nextInt(0, 100) > 70) {
                        mob->setArmor((ArmorSlot) 0, ItemStack(VanillaItemNames::DiamondHelmet.getString(), 1, 0, nullptr));
                        mob->setArmor((ArmorSlot) 1, ItemStack(VanillaItemNames::DiamondChestplate.getString(), 1, 0, nullptr));
                        mob->setArmor((ArmorSlot) 2, ItemStack(VanillaItemNames::DiamondLeggings.getString(), 1, 0, nullptr));
                        mob->setArmor((ArmorSlot) 3, ItemStack(VanillaItemNames::DiamondBoots.getString(), 1, 0, nullptr));
                        if (mob->getRandom().nextBoolean()) {
                            auto const &instance = mob->getAttribute(SharedAttributes::HEALTH);
                            auto &p = (AttributeInstance &) instance;
                            p.setMaxValue(50);
                            p.setCurrentValue(50);
                        }
                    }
                }
            }
        } else if (name == "minecraft:skeleton") {
            if (mob->getRandom().nextInt(0, 100) > 90) {
                mob->addTag("SuperMob");
                ItemStack itemStack("minecraft:bow", 1, 0, nullptr);
                unique_ptr<ListTag> listTag = ListTag::create();
                {
                    unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                    unique_ptr<CompoundTag> compoundTag = CompoundTag::create();
                    compoundTag->putShort("id", EnchantType::durability);
                    compoundTag->putShort("lvl", MINSHORT);
                    listTag->add(std::move(compoundTag));
                }
                {
                    unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                    unique_ptr<CompoundTag> compoundTag = CompoundTag::create();
                    compoundTag->putShort("id", EnchantType::arrowInfinite);
                    compoundTag->putShort("lvl", MINSHORT);
                    listTag->add(std::move(compoundTag));
                }
                {
                    unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                    unique_ptr<CompoundTag> compoundTag = CompoundTag::create();
                    compoundTag->putShort("id", EnchantType::damage_undead);
                    compoundTag->putShort("lvl", MINSHORT);
                    listTag->add(std::move(compoundTag));
                }
                if (itemStack.getUserData()) {
                    itemStack.getUserData()->put(ItemStack::TAG_ENCHANTS, std::move(listTag));
                } else {
                    unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                    unique_ptr<CompoundTag> compoundTag = CompoundTag::create();
                    compoundTag->put(ItemStack::TAG_ENCHANTS, std::move(listTag));
                    itemStack.setUserData(std::move(compoundTag));
                }
                mob->setCarriedItem(itemStack);
                mob->setOffhandSlot(itemStack);
                if (mob->getRandom().nextInt(0, 100) > 60) {
                    mob->setNameTag("VanessaSpy");
                }
            }
        } else if (name == "minecraft:spider") {
            if (mob->getRandom().nextInt(0, 100) > 70) {
                mob->addTag("SuperMob");
                mob->setNameTag("蜘蛛侠");
                {
                    auto const &instance = mob->getAttribute(SharedAttributes::MOVEMENT_SPEED);
                    auto &p = (AttributeInstance &) instance;
                    p.setMaxValue(10);
                    p.setCurrentValue(10);
                }
                {
                    auto const &instance = mob->getAttribute(SharedAttributes::LAVA_MOVEMENT_SPEED);
                    auto &p = (AttributeInstance &) instance;
                    p.setMaxValue(10);
                    p.setCurrentValue(10);
                }
                {
                    auto const &instance = mob->getAttribute(SharedAttributes::UNDERWATER_MOVEMENT_SPEED);
                    auto &p = (AttributeInstance &) instance;
                    p.setMaxValue(10);
                    p.setCurrentValue(10);
                }
            }
        } else if (name == "minecraft:wolf") {
            if (mob->getRandom().nextInt(0, 100) > 45) {
                mob->setNameTag("VanessaSpy");
            }
        } else if (name == "minecraft:phantom") {
            mob->setNameTag("提醒睡觉小助手");
            {
                auto const &instance = mob->getAttribute(SharedAttributes::MOVEMENT_SPEED);
                auto &p = (AttributeInstance &) instance;
                p.setMaxValue(0.1f);
                p.setCurrentValue(0.1f);
            }
        } else if (name == "minecraft:sheep") {
            if (mob->getRandom().nextInt(0, 100) > 70) {
                mob->setNameTag("jeb_");
            }
        }
        Level &level = source.getLevel();
        if (BloodMoon) {
            try {
                {
                    auto const &instance = mob->getAttribute(SharedAttributes::HEALTH);
                    auto &p = (AttributeInstance &) instance;
                    p.setMaxValue(p.getMaxValue() + p.getMaxValue() * 10 / 5);
                    p.setCurrentValue(p.getCurrentValue() + p.getCurrentValue() * 10 / 5);
                }
                {
                    auto const &instance = mob->getAttribute(SharedAttributes::MOVEMENT_SPEED);
                    auto &p = (AttributeInstance &) instance;
                    p.setMaxValue(p.getMaxValue() + p.getMaxValue() * 10 / 7);
                    p.setCurrentValue(p.getCurrentValue() + p.getCurrentValue() * 10 / 7);
                }
                {
                    auto const &instance = mob->getAttribute(SharedAttributes::LAVA_MOVEMENT_SPEED);
                    auto &p = (AttributeInstance &) instance;
                    p.setMaxValue(p.getMaxValue() + p.getMaxValue() * 10 / 7);
                    p.setCurrentValue(p.getCurrentValue() + p.getCurrentValue() * 10 / 7);
                }
                {
                    auto &p = Utils::getEntityAttribute(*mob, SharedAttributes::UNDERWATER_MOVEMENT_SPEED);
                    p.setMaxValue(p.getMaxValue() + p.getMaxValue() * 10 / 7);
                    p.setCurrentValue(p.getCurrentValue() + p.getCurrentValue() * 10 / 7);
                }
                for (int i = 0, max = mob->getRandom().nextInt(2, 5); i < max; ++i) {
                    ActorUniqueID uniqueId = level.getNewUniqueID();
                    Actor *actor = CommandUtils::spawnEntityAt(source, event.mPos, identifier.getCanonicalName(), uniqueId, nullptr);
                    actor->addTag("BloodMoon");
                    {
                        auto const &instance = actor->getAttribute(SharedAttributes::HEALTH);
                        auto &p = (AttributeInstance &) instance;
                        p.setMaxValue(p.getMaxValue() + p.getMaxValue() * 10 / 2);
                        p.setCurrentValue(p.getCurrentValue() + p.getCurrentValue() * 10 / 2);
                    }
                    {
                        auto const &instance = actor->getAttribute(SharedAttributes::MOVEMENT_SPEED);
                        auto &p = (AttributeInstance &) instance;
                        p.setMaxValue(p.getMaxValue() + p.getMaxValue() * 10 / 7);
                        p.setCurrentValue(p.getCurrentValue() + p.getCurrentValue() * 10 / 7);
                    }
                    {
                        auto const &instance = actor->getAttribute(SharedAttributes::LAVA_MOVEMENT_SPEED);
                        auto &p = (AttributeInstance &) instance;
                        p.setMaxValue(p.getMaxValue() + p.getMaxValue() * 10 / 7);
                        p.setCurrentValue(p.getCurrentValue() + p.getCurrentValue() * 10 / 7);
                    }
                    {
                        auto const &instance = actor->getAttribute(SharedAttributes::UNDERWATER_MOVEMENT_SPEED);
                        auto &p = (AttributeInstance &) instance;
                        p.setMaxValue(p.getMaxValue() + p.getMaxValue() * 10 / 7);
                        p.setCurrentValue(p.getCurrentValue() + p.getCurrentValue() * 10 / 7);
                    }
                }
                mob->addTag("BloodMoon");
            } catch (exception &e) {
                logger.error("发生异常", e.what());
            }
        }
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
    Event::PlayerCmdEvent::subscribe_ref([](auto &event) {
        logger.info("cmd {}", event.mCommand);
        if (event.mCommand == "me 血月启动") {
            Level::runcmd("/time set night");
            BloodMoon = true;
            Level::broadcastText("§c血月升起了！", TextType::SYSTEM);
            logger.info("血月升起");
        }
        return true;
    });
}

TInstanceHook(void, "?_subTick@ServerLevel@@MEAAXXZ", ServerLevel) {
    int day = LevelUtils::getDay(getTime());
    int time = LevelUtils::getTimeOfDay(getTime());
    if (time == 13000 && !getPlayerList().empty()) {
        if (day % 30 == 0) {
            if (getRandom().nextInt(0, 100) > 50) {
                BloodMoon = true;
                Level::broadcastText("§c血月升起了！", TextType::SYSTEM);
                logger.info("血月升起");
            }
        }
    } else if (time == 18000 && BloodMoon) {
        Level::broadcastText("§c血月已到达中期！", TextType::SYSTEM);
    } else if ((time < 13000 || time == 23999) && BloodMoon) {
        Level::broadcastText("§e血月结束了！", TextType::SYSTEM);
        logger.info("血月结束");
        for (auto &p: Level::getAllPlayers())p->sendBossEventPacket(BossEvent::Hide, string(), 0, BossEventColour::Red);
        Schedule::delay([]() {
            for (auto &it: Level::getAllEntities())if (it->hasTag("BloodMoon"))it->remove();
        }, 20 * 5);
        BloodMoon = false;
    } else if (BloodMoon) {
        float startTime = time - 13000.0f;
        float endTime = 23999.0f - 13000.0f;
        float percentage = (startTime / endTime);
        for (auto &p: Level::getAllPlayers())p->sendBossEventPacket(BossEvent::Show, "§c血月", percentage, BossEventColour::Red);
    }
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

// Player::startSleepInBed(BlockPos const &)
TInstanceHook(BedSleepingResult, "?startSleepInBed@Player@@UEAA?AW4BedSleepingResult@@AEBVBlockPos@@@Z", Player, BlockPos const &pos) {
    if (BloodMoon) {
        static vector<string> texts = {"§e你怎么睡得着的？", "§e你这个年龄段，你睡得着觉？", "§e有僵尸偷你东西！", "§e什么？你想睡觉？"};
        int index = getRandom().nextInt(0, texts.size());
        sendText(texts[index], TextType::TIP);
        return (BedSleepingResult) 0;
    }
    return original(this, pos);
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

//ServerNetworkHandler::handle(NetworkIdentifier const &,RequestNetworkSettingsPacket const &)
TInstanceHook(void, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVRequestNetworkSettingsPacket@@@Z", ServerNetworkHandler, NetworkIdentifier &identifier, RequestNetworkSettingsPacket & packet) {
    int client = dAccess<int, 48>(&packet);
    int protocol = SharedConstants::NetworkProtocolVersion;
    logger.info("RequestNetworkSettingsPacket {} {}", client, protocol);
    return original(this, identifier, packet);
}

TInstanceHook(void, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVLoginPacket@@@Z", ServerNetworkHandler, NetworkIdentifier &identifier, LoginPacket & packet) {
    int *client = (int *) ((uintptr_t) &packet + 48);
    int protocol = SharedConstants::NetworkProtocolVersion;
    logger.info("LoginPacket {} {}", *client, protocol);
    return original(this, identifier, packet);
}


struct ContainerEnumNameHasher {
public:
    std::size_t operator()(ContainerEnumName k) const {
        return 0x9FFAAC085635BC91 * ((unsigned __int8) k ^ 0xCBF29CE484222325);
    }
};

class ItemStackResponseSlotInfo {
public:
    uint8_t slot;
    uint8_t hotbarSlot;
    uint8_t count;
    TypedServerNetId<ItemStackNetIdTag, int, 0> itemStackId;
    std::string customName;
    int durabilityCorrection;
public:
    string toString() {
        stringstream ss;
        ss << "slot: " << (int) slot << " hotbarSlot: " << (int) hotbarSlot << " count: " << (int) count;
        ss << "itemStackId:" << itemStackId.netId << " ";
        ss << "customName:" << customName << " ";
        ss << "durabilityCorrection:" << durabilityCorrection;
        return ss.str();
    }
};

class ItemStackResponseContainerInfo {
public:
    ContainerEnumName containerId;
    std::vector<ItemStackResponseSlotInfo> slots;
public:
    ItemStackResponseContainerInfo(ContainerEnumName);

    ItemStackResponseContainerInfo(ItemStackResponseContainerInfo &&);

    ItemStackResponseContainerInfo &operator=(ItemStackResponseContainerInfo &&);

public:
    string toString() {
        std::unordered_map<ContainerEnumName, std::string, ContainerEnumNameHasher> &map = *(std::unordered_map<ContainerEnumName, std::string, ContainerEnumNameHasher> *) dlsym_real("?ContainerCollectionNameMap@@3V?$unordered_map@W4ContainerEnumName@@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@UContainerEnumNameHasher@@U?$equal_to@W4ContainerEnumName@@@3@V?$allocator@U?$pair@$$CBW4ContainerEnumName@@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@std@@@3@@std@@A");

        std::stringstream ss;
        ss << "container:" << map.at(containerId) << endl;
        ss << "slots:";
        for (auto &item: slots) {
            ss << item.toString() << endl;
        }
        return ss.str();
    }
};

struct ItemStackResponseInfo {
    enum class Result : bool {
        OK, ERROR
    } result;
    TypedClientNetId<ItemStackRequestIdTag, int, 0> netId;
    std::vector<ItemStackResponseContainerInfo> responseContainerInfoList;

    ItemStackResponseInfo();

    ItemStackResponseInfo(ItemStackResponseInfo &&);

    ItemStackResponseInfo &operator=(ItemStackResponseInfo &&);

    string toString() {
        std::stringstream ss;
        ss << "result:" << (int) result << " ";
        ss << "netId:" << netId.netId << " ";
        ss << "list:";
        for (auto &item: responseContainerInfoList) {
            ss << item.toString() << endl;
        }
        return ss.str();
    }
};

TInstanceHook(void, "?send@NetworkSystem@@QEAAXAEBVNetworkIdentifier@@AEBVPacket@@W4SubClientId@@@Z", NetworkSystem, NetworkIdentifier const &identifier, Packet const &packet, SubClientId id) {
    //logger.info("Sending {}",packet.getName());
    if (packet.getId() == MinecraftPacketIds::PlayerList) {
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
        auto ptr = (ContainerClosePacket *) &packet;
        ContainerID containerId = dAccess<ContainerID, 48>(ptr);
        bool b = dAccess<bool, 49>(ptr);
        logger.info("ContainerClose {} {}", (int) containerId, b);
        // 2 false
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
    }
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
