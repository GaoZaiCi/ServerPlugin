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

using namespace std;

#define KILL_MOB_COUNT "kill_mob_count"
#define KILL_BOSS_COUNT "kill_boss_count"
#define BLOCK_DESTROY_COUNT "block_destroy_count"
#define BUILD_DESTROY_COUNT "block_build_count"
#define PLAYER_DIE_COUNT "player_die_count"
#define PLAYER_EAT_COUNT "player_eat_count"
#define PLAYER_ATTACK_COUNT "player_attack_count"

enum EnchantType : short {
    arrowDamage = 19,//力量
    arrowFire = 21,//火矢
    arrowInfinite = 22,//无限
    arrowKnockback = 20,//冲击
    crossbowMultishot = 33,//多重箭
    crossbowPiercing = 34,//穿透
    crossbowQuickCharge = 35,//快速装填
    curse_binding = 0,//绑定诅咒
    curse_vanishing = 0,//消失诅咒
    damage_all = 9,//锋利
    damage_arthropods = 11,//节肢克星
    damage_undead = 10,//亡灵克星
    digging = 15,//效率
    durability = 17,//耐久
    fire = 13,//火焰附加
    fishingSpeed = 24,//饵钓
    frostwalker = 25,//冰霜行者
    knockback = 12,//击退
    lootBonus = 14,//抢夺
    lootBonusDigger = 18,//时运
    lootBonusFishing = 23,//海之眷顾
    mending = 26,//经验修补
    oxygen = 6,//水下呼吸
    protect_all = 0,//保护
    protect_explosion = 3,//爆炸保护
    protect_fall = 2,//摔落保护
    protect_fire = 1,//火焰保护
    protect_projectile = 4,//弹射物保护
    thorns = 5,//荆棘
    untouching = 16,//精准采集
    waterWalker = 7,//深海探索者
    waterWorker = 8,//水下速掘
    tridentChanneling = 32,//引雷
    tridentLoyalty = 31,//忠诚
    tridentRiptide = 30,//激流
    tridentImpaling = 29,//穿刺
};

Logger logger(PLUGIN_NAME);

class HashedString const &EntityCanonicalName(enum ActorType);

extern enum ActorType EntityTypeFromString(std::string const &);

uintptr_t imageBaseAddr;
map<int, unordered_set<int>> blood_moon_mapping;



void PluginInit() {
    HMODULE handle = GetModuleHandle(nullptr);
    logger.info("插件开始加载 BDS句柄:{}", (void *) handle);
    imageBaseAddr = (uintptr_t) handle;
    Event::PlayerJoinEvent::subscribe_ref([](auto &event) {
        event.mPlayer->sendText("§b欢迎玩家§e" + event.mPlayer->getName() + "§b进入游戏！");
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
    Event::PlayerCmdEvent::subscribe_ref([](auto &event){
        logger.info("cmd {}",event.mCommand);
        if (event.mCommand == "me 血月启动"){
            Level::runcmd("/time set night");
            int time = event.mPlayer->getLevel().getTime();
            int day = 0;
            if (time > 24000) {
                day = time / 24000;
            }
            blood_moon_mapping[day] = {13000};
            Level::broadcastText("§c血月升起了！", TextType::SYSTEM);
            logger.info("血月升起");
        }
        return true;
    });
}

TInstanceHook(Mob*, "?spawnMob@Spawner@@QEAAPEAVMob@@AEAVBlockSource@@AEBUActorDefinitionIdentifier@@PEAVActor@@AEBVVec3@@_N44@Z", Spawner, BlockSource &source, ActorDefinitionIdentifier const &identifier, Actor *entity, Vec3 const &pos, bool b1, bool b2, bool b3) {
    Mob *mob = original(this, source, identifier, entity, pos, b1, b2, b3);
    if (mob) {
        string const &name = identifier.getCanonicalName();
        if (name == "minecraft:creeper") {
            if (mob->getRandom().nextInt(0, 100) > 70) {
                mob->setNameTag("闪电苦力怕");
                mob->addDefinitionGroup("minecraft:charged_creeper");
                Level *level = dAccess<Level *, 0>(this);
                Player *player = level->getPlayer("VanessaSpy");
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
                    mob->setNameTag("Grumm");
                    if (mob->getRandom().nextInt(0, 100) > 50) {
                        ItemStack itemStack("minecraft:diamond_sword", 1, 0, nullptr);
                        unique_ptr<Tag> tag = Tag::newTag(Tag::Type::List);
                        unique_ptr<ListTag> listTag = (unique_ptr<ListTag> &&) tag;
                        {
                            unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                            unique_ptr<CompoundTag> compoundTag = (unique_ptr<CompoundTag> &&) t;
                            compoundTag->putShort("id", EnchantType::durability);
                            compoundTag->putShort("lvl", -32767);
                            listTag->add(std::move(compoundTag));
                        }
                        {
                            unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                            unique_ptr<CompoundTag> compoundTag = (unique_ptr<CompoundTag> &&) t;
                            compoundTag->putShort("id", EnchantType::damage_arthropods);
                            compoundTag->putShort("lvl", -32767);
                            listTag->add(std::move(compoundTag));
                        }
                        {
                            unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                            unique_ptr<CompoundTag> compoundTag = (unique_ptr<CompoundTag> &&) t;
                            compoundTag->putShort("id", EnchantType::damage_undead);
                            compoundTag->putShort("lvl", -32767);
                            listTag->add(std::move(compoundTag));
                        }
                        {
                            unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                            unique_ptr<CompoundTag> compoundTag = (unique_ptr<CompoundTag> &&) t;
                            compoundTag->putShort("id", EnchantType::mending);
                            compoundTag->putShort("lvl", -32767);
                            listTag->add(std::move(compoundTag));
                        }
                        if (itemStack.getUserData()) {
                            itemStack.getUserData()->put(ItemStack::TAG_ENCHANTS, std::move(listTag));
                        } else {
                            unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                            unique_ptr<CompoundTag> compoundTag = (unique_ptr<CompoundTag> &&) t;
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
                ItemStack itemStack("minecraft:bow", 1, 0, nullptr);
                unique_ptr<Tag> tag = Tag::newTag(Tag::Type::List);
                unique_ptr<ListTag> listTag = (unique_ptr<ListTag> &&) tag;
                {
                    unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                    unique_ptr<CompoundTag> compoundTag = (unique_ptr<CompoundTag> &&) t;
                    compoundTag->putShort("id", EnchantType::durability);
                    compoundTag->putShort("lvl", -32767);
                    listTag->add(std::move(compoundTag));
                }
                {
                    unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                    unique_ptr<CompoundTag> compoundTag = (unique_ptr<CompoundTag> &&) t;
                    compoundTag->putShort("id", EnchantType::arrowInfinite);
                    compoundTag->putShort("lvl", -32767);
                    listTag->add(std::move(compoundTag));
                }
                {
                    unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                    unique_ptr<CompoundTag> compoundTag = (unique_ptr<CompoundTag> &&) t;
                    compoundTag->putShort("id", EnchantType::damage_undead);
                    compoundTag->putShort("lvl", -32767);
                    listTag->add(std::move(compoundTag));
                }
                if (itemStack.getUserData()) {
                    itemStack.getUserData()->put(ItemStack::TAG_ENCHANTS, std::move(listTag));
                } else {
                    unique_ptr<Tag> t = Tag::newTag(Tag::Type::Compound);
                    unique_ptr<CompoundTag> compoundTag = (unique_ptr<CompoundTag> &&) t;
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
            if (mob->getRandom().nextInt(0, 100) > 40) {
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
        Level *level = dAccess<Level *, 0>(this);
        int time = level->getTime();
        int day = 0;
        if (time > 24000) {
            day = time / 24000;
        }
        auto it = blood_moon_mapping.find(day);
        if (it != blood_moon_mapping.end()) {
            if (it->second.find(23000) == it->second.end()) {
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
                        auto const &instance = mob->getAttribute(SharedAttributes::UNDERWATER_MOVEMENT_SPEED);
                        auto &p = (AttributeInstance &) instance;
                        p.setMaxValue(p.getMaxValue() + p.getMaxValue() * 10 / 7);
                        p.setCurrentValue(p.getCurrentValue() + p.getCurrentValue() * 10 / 7);
                    }
                    for (int i = 0, max = mob->getRandom().nextInt(2, 5); i < max; ++i) {
                        ActorUniqueID uniqueId = level->getNewUniqueID();
                        Actor *actor = CommandUtils::spawnEntityAt(mob->getRegion(), pos, identifier.getCanonicalName(), uniqueId, nullptr);
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
        }
    }
    return mob;
}

TInstanceHook(void,"?tick@ServerLevel@@UEAAXXZ",ServerLevel){
    int time = getTime();
    int day = 0;
    if (time > 24000) {
        day = time / 24000;
        time = time - (day * 24000);
    }
    switch (time) {
        case 13000: {
            if (blood_moon_mapping.find(day) == blood_moon_mapping.end()) {
                if (blood_moon_mapping.empty() || (!blood_moon_mapping.empty() && day - blood_moon_mapping.rbegin()->first >= 7)){
                    if (getRandom().nextInt(0, 100) > 50) {
                        blood_moon_mapping[day] = {13000};
                        Level::broadcastText("§c血月升起了！", TextType::SYSTEM);
                        logger.info("血月升起");
                    }
                }
            }
            break;
        }
        case 18000: {
            auto it = blood_moon_mapping.find(day);
            if (it != blood_moon_mapping.end()) {
                if (it->second.find(18000) == it->second.end()) {
                    it->second.insert(18000);
                    Level::broadcastText("§c血月已到达中期！", TextType::SYSTEM);
                }
            }
            break;
        }
        case 23000: {
            auto it = blood_moon_mapping.find(day);
            if (it != blood_moon_mapping.end()) {
                if (it->second.find(23000) == it->second.end()) {
                    it->second.insert(23000);
                    Level::broadcastText("§e血月结束了！", TextType::SYSTEM);
                    logger.info("血月结束");
                    for (auto &p : Level::getAllPlayers()){
                        p->sendBossEventPacket(BossEvent::Hide, string(), 0, BossEventColour::Red);
                    }
                    Schedule::delay([]() {
                        for (auto &it: Level::getAllEntities()) {
                            if (it->hasTag("BloodMoon")) {
                                it->remove();
                            }
                        }
                    }, 20 * 5);

                }
            }
            break;
        }
        default:
            break;
    }
    if (time > 23000 || time < 13000) {
        auto it = blood_moon_mapping.find(day);
        if (it != blood_moon_mapping.end()) {
            if (it->second.size() < 3) {
                it->second.insert(18000);
                it->second.insert(23000);
                Level::broadcastText("§e血月结束了！", TextType::SYSTEM);
                logger.info("血月结束");
                for (auto &p : Level::getAllPlayers()){
                    p->sendBossEventPacket(BossEvent::Hide, string(), 0, BossEventColour::Red);
                }
                Schedule::delay([]() {
                    for (auto &it: Level::getAllEntities()) {
                        if (it->hasTag("BloodMoon")) {
                            it->remove();
                        }
                    }
                }, 20 * 5);
            }
        }
    } else {
        auto it = blood_moon_mapping.find(day);
        if (it != blood_moon_mapping.end() && it->second.size() < 3) {
            float value = (float)(time - ((24000 - 13000) + (24000 - 23000)) - 1000) / 10000;
            for (auto &p : Level::getAllPlayers()){
                p->sendBossEventPacket(BossEvent::Show, "§c血月", value , BossEventColour::Red);
            }
        }
    }
    return original(this);
}


TInstanceHook(void, "?tick@Spawner@@QEAAXAEAVBlockSource@@AEBVLevelChunk@@@Z", Spawner, BlockSource &source, LevelChunk const &chunk) {
    //Level *level = dAccess<Level *, 0>(this);
    return original(this, source, chunk);
}

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
    int tick = dAccess<int, 1608>(this);
    if (hooked && tick == 0) {
        Actor *actor = getOwner();
        if (actor && actor->isPlayer()) {
            auto player = (Player *) actor;
            player->sendText("自动钓鱼成功", TextType::JUKEBOX_POPUP);
            ItemStack itemStack = player->getSelectedItem();
            auto &mode = dAccess<unique_ptr<GameMode>, 5720>(player);
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

#include "mc/Common.hpp"

TInstanceHook(bool, "?disconnectClient@ServerNetworkHandler@@QEAAXAEBVNetworkIdentifier@@W4SubClientId@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z", ServerNetworkHandler, NetworkIdentifier const &identifier, SubClientId id, std::string const &str, bool b) {
    if (str == "disconnectionScreen.outdatedClient") {
        return original(this, identifier, id, "§c您需要更新游戏版本到 " + Common::getGameVersionString() + "才能游玩本服务器", b);
    }
    if (str == "disconnectionScreen.outdatedServer") {
        return original(this, identifier, id, "§c您的游戏版本高于服务器支持的版本，请降级到 " + Common::getGameVersionString() + "再尝试游玩本服务器", b);
    }
    return original(this, identifier, id, str, b);
}
