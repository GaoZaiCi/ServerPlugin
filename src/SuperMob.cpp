//
// Created by ASUS on 2023/11/17.
//

#include "SuperMob.h"
#include "Utils.h"

#include "mc/Level.hpp"
#include "mc/Player.hpp"
#include "mc/Mob.hpp"
#include "mc/BlockSource.hpp"
#include "mc/ActorDefinitionIdentifier.hpp"
#include "mc/Random.hpp"
#include "mc/SharedAttributes.hpp"
#include "mc/AttributeInstance.hpp"
#include "mc/ItemStack.hpp"
#include "mc/ListTag.hpp"
#include "mc/VanillaItemNames.hpp"

SuperMob::SuperMob() : logger(__FILE__) {
}

void SuperMob::init() {
    MobSpawnedEvent::subscribe_ref([this](auto &event) {
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
                        mob->setArmor((ArmorSlot) 0, ItemStack(VanillaItemNames::DiamondHelmet, 1, 0, nullptr));
                        mob->setArmor((ArmorSlot) 1, ItemStack(VanillaItemNames::DiamondChestplate, 1, 0, nullptr));
                        mob->setArmor((ArmorSlot) 2, ItemStack(VanillaItemNames::DiamondLeggings, 1, 0, nullptr));
                        mob->setArmor((ArmorSlot) 3, ItemStack(VanillaItemNames::DiamondBoots, 1, 0, nullptr));
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
        return true;
    });
}
