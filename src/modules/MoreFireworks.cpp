//
// Created by ASUS on 2023/11/21.
//

#include "MoreFireworks.h"
#include "Utils.h"

#include "EventAPI.h"

#include "mc/VanillaItemNames.hpp"
#include "mc/ItemLockHelper.hpp"
#include "mc/ItemStack.hpp"
#include "mc/ItemInstance.hpp"
#include "mc/BaseGameVersion.hpp"
#include "mc/SemVersion.hpp"
#include "mc/Level.hpp"
#include "mc/LevelData.hpp"
#include "mc/Recipe.hpp"
#include "mc/Recipes.hpp"
#include "mc/CompoundTag.hpp"
#include "mc/CraftingTag.hpp"
#include "mc/ItemRegistryRef.hpp"
#include "mc/HashedString.hpp"
#include "mc/FireworksItem.hpp"

using namespace Event;

extern MoreFireworks mMoreFireworks;

MoreFireworks::MoreFireworks() : logger(__FILE__) {
}

void MoreFireworks::init() {
    ServerStartedEvent::subscribe_ref([](auto &event) {
        {
            ItemStack itemStack("minecraft:fireworks", 1, 0, nullptr);
            Utils::enchant(itemStack, EnchantType::durability, MINSHORT);
            auto Fireworks = Utils::makeTag<CompoundTag>(Tag::Type::Compound);
            Fireworks->putByte(FireworksItem::TAG_E_FLIGHT, 5);
            itemStack.getUserData()->putCompound(FireworksItem::TAG_FIREWORKS, std::move(Fireworks));

            auto &baseGameVersion = Global<Level>->getLevelData().getBaseGameVersion();
            auto version = baseGameVersion.asSemVersion();
            Global<Level>->getRecipes().addShapedRecipe("minecraft:fireworks_5", itemStack, {"AB"}, {
                    {"minecraft:fireworks", 'A', 1, 0},
                    {"minecraft:tnt",       'B', 1, 0}
            }, {CraftingTag::CRAFTING_TABLE}, 2, nullptr, nullopt, version);
        }
        {
            ItemStack itemStack("minecraft:fireworks", 1, 0, nullptr);
            Utils::enchant(itemStack, EnchantType::durability, MINSHORT);
            auto Fireworks = Utils::makeTag<CompoundTag>(Tag::Type::Compound);
            Fireworks->putByte(FireworksItem::TAG_E_FLIGHT, 10);
            itemStack.getUserData()->putCompound(FireworksItem::TAG_FIREWORKS, std::move(Fireworks));

            auto &baseGameVersion = Global<Level>->getLevelData().getBaseGameVersion();
            auto version = baseGameVersion.asSemVersion();
            Global<Level>->getRecipes().addShapedRecipe("minecraft:fireworks_10", itemStack, {"AB","B"}, {
                    {"minecraft:fireworks", 'A', 1, 0},
                    {"minecraft:tnt",       'B', 1, 0}
            }, {CraftingTag::CRAFTING_TABLE}, 2, nullptr, nullopt, version);
        }
        {
            ItemStack itemStack("minecraft:fireworks", 1, 0, nullptr);
            Utils::enchant(itemStack, EnchantType::durability, MINSHORT);
            auto Fireworks = Utils::makeTag<CompoundTag>(Tag::Type::Compound);
            Fireworks->putByte(FireworksItem::TAG_E_FLIGHT, 20);
            itemStack.getUserData()->putCompound(FireworksItem::TAG_FIREWORKS, std::move(Fireworks));

            auto &baseGameVersion = Global<Level>->getLevelData().getBaseGameVersion();
            auto version = baseGameVersion.asSemVersion();
            Global<Level>->getRecipes().addShapedRecipe("minecraft:fireworks_20", itemStack, {"ABB","BB","B"}, {
                    {"minecraft:fireworks", 'A', 1, 0},
                    {"minecraft:tnt",       'B', 1, 0}
            }, {CraftingTag::CRAFTING_TABLE}, 2, nullptr, nullopt, version);
        }
        {
            ItemStack itemStack("minecraft:fireworks", 1, 0, nullptr);
            Utils::enchant(itemStack, EnchantType::durability, MINSHORT);
            auto Fireworks = Utils::makeTag<CompoundTag>(Tag::Type::Compound);
            Fireworks->putByte(FireworksItem::TAG_E_FLIGHT, 40);
            itemStack.getUserData()->putCompound(FireworksItem::TAG_FIREWORKS, std::move(Fireworks));

            auto &baseGameVersion = Global<Level>->getLevelData().getBaseGameVersion();
            auto version = baseGameVersion.asSemVersion();
            Global<Level>->getRecipes().addShapedRecipe("minecraft:fireworks_40", itemStack, {"ABB","BBB","BBB"}, {
                    {"minecraft:fireworks", 'A', 1, 0},
                    {"minecraft:tnt",       'B', 1, 0}
            }, {CraftingTag::CRAFTING_TABLE}, 2, nullptr, nullopt, version);
        }
        return true;
    });
}
