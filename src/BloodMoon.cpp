//
// Created by ASUS on 2023/11/17.
//

#include "BloodMoon.h"
#include "Utils.h"

#include "mc/ListTag.hpp"
#include "mc/ItemStack.hpp"
#include "mc/Random.hpp"
#include "mc/Level.hpp"
#include "mc/ServerLevel.hpp"
#include "mc/Mob.hpp"
#include "mc/Player.hpp"
#include "mc/SharedAttributes.hpp"
#include "mc/BlockSource.hpp"
#include "mc/ActorDefinitionIdentifier.hpp"
#include "mc/CommandUtils.hpp"
#include "mc/LevelUtils.hpp"
#include "ScheduleAPI.h"

extern BloodMoon mBloodMoon;

BloodMoon::BloodMoon() : logger(__FILE__) {}

void BloodMoon::init() {
    MobSpawnedEvent::subscribe_ref([this](auto &event) {
        if (this->enabled) {
            auto mob = event.mMob;
            auto &source = mob->getRegion();
            auto identifier = mob->getActorIdentifier();
            auto &level = source.getLevel();
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
        }
        return true;
    });
    PlayerCmdEvent::subscribe_ref([this](auto &event) {
        if (event.mCommand == "me 血月启动") {
            Level::runcmd("/time set night");
            this->enabled = true;
            Level::broadcastText("§c血月升起了！", TextType::SYSTEM);
            logger.info("血月升起");
        }
        return true;
    });
    ConsoleCmdEvent::subscribe_ref([this](auto &event) {
        if (event.mCommand == "me 血月启动") {
            Level::runcmd("/time set night");
            this->enabled = true;
            Level::broadcastText("§c血月升起了！", TextType::SYSTEM);
            logger.info("血月升起");
        }
        return true;
    });
}

void BloodMoon::onTick(ServerLevel *level) {
    int day = LevelUtils::getDay(level->getTime());
    int time = LevelUtils::getTimeOfDay(level->getTime());
    if (time == 13000 && !level->getPlayerList().empty()) {
        if (day % 7 == 0) {
            if (level->getRandom().nextInt(0, 100) > 50) {
                this->enabled = true;
                Level::broadcastText("§c血月升起了！", TextType::SYSTEM);
                logger.info("血月升起");
            }
        }
    } else if (time == 18000 && this->enabled) {
        Level::broadcastText("§c血月已到达中期！", TextType::SYSTEM);
    } else if ((time < 13000 || time == 23999) && this->enabled) {
        Level::broadcastText("§e血月结束了！", TextType::SYSTEM);
        logger.info("血月结束");
        for (auto &p: Level::getAllPlayers())p->sendBossEventPacket(BossEvent::Hide, string(), 0, BossEventColour::Red);
        Schedule::delay([]() {
            for (auto &it: Level::getAllEntities())if (it->hasTag("BloodMoon"))it->remove();
        }, 20 * 5);
        this->enabled = false;
    } else if (this->enabled) {
        float startTime = time - 13000.0f;
        float endTime = 23999.0f - 13000.0f;
        float percentage = (startTime / endTime);
        for (auto &p: Level::getAllPlayers())p->sendBossEventPacket(BossEvent::Show, "§c血月", percentage, BossEventColour::Red);
    }
}

// Player::startSleepInBed(BlockPos const &)
TInstanceHook(BedSleepingResult, "?startSleepInBed@Player@@UEAA?AW4BedSleepingResult@@AEBVBlockPos@@@Z", Player, BlockPos const &pos) {
    if (mBloodMoon.enabled) {
        int index = getRandom().nextInt(0, mBloodMoon.texts.size());
        sendText(mBloodMoon.texts[index], TextType::TIP);
        return (BedSleepingResult) 0;
    }
    return original(this, pos);
}
