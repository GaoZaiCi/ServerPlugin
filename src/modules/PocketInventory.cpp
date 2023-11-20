//
// Created by ASUS on 2023/11/13.
//
#include <iostream>
#include <filesystem>

#include "PocketInventory.h"

#include "llapi/EventAPI.h"
#include "llapi/ScheduleAPI.h"
#include "mc/VanillaBlockTypeIds.hpp"
#include "mc/LoopbackPacketSender.hpp"
#include "mc/ContainerOpenPacket.hpp"
#include "mc/InventorySlotPacket.hpp"
#include "mc/ContainerClosePacket.hpp"
#include "mc/NetworkItemStackDescriptor.hpp"
#include "mc/Level.hpp"
#include "mc/BlockPos.hpp"
#include "mc/ActorUniqueID.hpp"
#include "mc/ItemStack.hpp"
#include "mc/ItemInstance.hpp"
#include "mc/Item.hpp"
#include "mc/CompoundTag.hpp"
#include "mc/ListTag.hpp"
#include "mc/ServerPlayer.hpp"
#include "mc/Block.hpp"
#include "mc/HashedString.hpp"
#include "mc/ItemStackResponsePacket.hpp"
#include "mc/PlayerInventory.hpp"
#include "mc/BedrockBlockNames.hpp"
#include "mc/BlockActor.hpp"
#include "mc/Inventory.hpp"
#include "mc/ItemRegistryRef.hpp"
#include "mc/LevelData.hpp"
#include "mc/Recipe.hpp"
#include "mc/Recipes.hpp"
#include "mc/SemVersion.hpp"
#include "mc/ItemLockHelper.hpp"
#include "mc/VanillaItemNames.hpp"
#include "mc/CraftingTag.hpp"
#include "mc/BlockSource.hpp"
#include "mc/LevelChunk.hpp"
#include "mc/MovingBlockActor.hpp"
#include "mc/Dimension.hpp"
#include "mc/BaseGameVersion.hpp"

using namespace Event;

extern PocketInventory mPocketInventory;

#define TRY_CREATE_DIR(path) if (!filesystem::exists(path))filesystem::create_directories(path);

PocketInventory::PocketInventory() : logger(__FILE__) {
    TRY_CREATE_DIR("plugins/NativeEnhancements/players")
}

void PocketInventory::init() {
    ServerStartedEvent::subscribe_ref([](auto &event) {
        ItemStack itemStack(VanillaItemNames::Saddle, 1, 0, nullptr);
        itemStack.setCustomName("§o§l§b移动背包");
        itemStack.setCustomLore({"§e右键或者长按打开背包"});
        itemStack.getUserData()->putBoolean("PocketInventory", true);
        Utils::enchant(itemStack, EnchantType::durability, MINSHORT);
        ItemLockHelper::setKeepOnDeath(itemStack, true);

        auto &baseGameVersion = Global<Level>->getLevelData().getBaseGameVersion();
        SemVersion version = baseGameVersion.asSemVersion();
        Global<Level>->getRecipes().addShapedRecipe("PL::operator", itemStack, {"AAA", "ABA", "AAA"}, {
                {VanillaItemNames::Leather, 'A', 1, 0},
                {VanillaItemNames::Diamond, 'B', 1, 0}
        }, {CraftingTag::CRAFTING_TABLE}, 2, nullptr, nullopt, version);
        return true;
    });
    PlayerJoinEvent::subscribe_ref([this](auto &event) {
        this->playerInventoryPageMap[event.mPlayer->getActorUniqueId()] = 0;
        loadPlayerData(event.mPlayer);
        return true;
    });
    PlayerLeftEvent::subscribe_ref([this](auto &event) {
        savePlayerData(event.mPlayer);
        this->playerInventoryMap.erase(event.mPlayer->getActorUniqueId());
        this->playerInventoryPageMap.erase(event.mPlayer->getActorUniqueId());
        return true;
    });
    PlayerUseItemEvent::subscribe_ref([this](auto &event) {
        if (event.mItemStack->hasUserData()) {
            if (event.mItemStack->getUserData()->contains("PocketInventory")) {
                openInventory(event.mPlayer);
            }
        }
        return true;
    });
}

void PocketInventory::openInventory(Player *player) {
    auto serverPlayer = (ServerPlayer *) player;
    auto it = this->inventoryMap.find(player->getNetworkIdentifier()->getHash());
    if (it != this->inventoryMap.end()) {
        player->sendText("§c当前背包已经打开");
        return;
    }

    BlockPos posA, posB;

    /*Level::setBlock(posA, player->getDimensionId(), VanillaBlockTypeIds::MovingBlock, 0);
    Level::setBlock(posB, player->getDimensionId(), VanillaBlockTypeIds::MovingBlock, 0);

    auto blockActorA = (MovingBlockActor *) Level::getBlockEntity(posA, player->getDimensionId());
    auto blockActorB = (MovingBlockActor *) Level::getBlockEntity(posB, player->getDimensionId());

    auto nbtA = CompoundTag::fromSNBT(R"({
            "movingBlock": {
            "name": "minecraft:light_block"
        },
            "movingBlockExtra": {
            "name": "minecraft:chest",
            "states":{"facing_direction":4}
        },
            "display": {
            "Name": ""
        }
        })");
    logger.info("nbt {}", nbtA->toSNBT());
    auto globalHelper = dlsym_real("??_7DefaultDataLoadHelper@@6B@");
    blockActorA->load(player->getLevel(), *nbtA, *(DataLoadHelper * ) & globalHelper);
    blockActorB->load(player->getLevel(), *nbtA, *(DataLoadHelper * ) & globalHelper);

    blockActorA->refreshData();
    blockActorB->refreshData();*/

    if (!getFakeChestPos(player, &posA, &posB)) {
        player->sendText("§c当前周围有障碍物，请在空旷的地方打开");
        return;
    }

    /*auto nbtA = Utils::makeTag<CompoundTag>(Tag::Type::Compound);
    if (!blockA->isAir()) {
        nbtA->putCompound("Block", CompoundTag::fromBlock(blockA));
        if (blockA->hasBlockEntity()) {
            auto blockEntity = Level::getBlockEntity(posA, player->getDimensionId());
            nbtA->putCompound("BlockEntity", CompoundTag::fromBlockActor(blockEntity));
        }
    }

    auto nbtB = Utils::makeTag<CompoundTag>(Tag::Type::Compound);
    if (!blockB->isAir()) {
        nbtB->putCompound("Block", CompoundTag::fromBlock(blockB));
        if (blockB->hasBlockEntity()) {
            auto blockEntity = Level::getBlockEntity(posB, player->getDimensionId());
            nbtB->putCompound("BlockEntity", CompoundTag::fromBlockActor(blockEntity));
        }
    }

    logger.info("A {}", nbtA->toSNBT());
    logger.info("B {}", nbtB->toSNBT());

    int dim = player->getDimensionId();
    {
        if (nbtA->contains("Block")) {
            Level::setBlock({posA.x, posA.y + 1, posA.z}, dim, nbtA->getCompound("Block"));
            if (nbtA->contains("BlockEntity")) {
                auto blockEntity = Level::getBlockEntity(posA, player->getDimensionId());
                if (blockEntity) {
                    auto globalHelper = dlsym_real("??_7DefaultDataLoadHelper@@6B@");
                    blockEntity->load(player->getLevel(), *nbtA->getCompound("BlockEntity"), *(DataLoadHelper * ) & globalHelper);
                }
            }
        }
        if (nbtB->contains("Block")) {
            Level::setBlock({posB.x, posB.y + 1, posB.z}, dim, nbtB->getCompound("Block"));
            if (nbtB->contains("BlockEntity")) {
                auto blockEntity = Level::getBlockEntity(posB, player->getDimensionId());
                if (blockEntity) {
                    auto globalHelper = dlsym_real("??_7DefaultDataLoadHelper@@6B@");
                    blockEntity->load(player->getLevel(), *nbtB->getCompound("BlockEntity"), *(DataLoadHelper * ) & globalHelper);
                }
            }
        }
    }*/


    Level::setBlock(posA, player->getDimensionId(), VanillaBlockTypeIds::Chest, 4);
    Level::setBlock(posB, player->getDimensionId(), VanillaBlockTypeIds::Chest, 4);

    BlockActor *chestBlockActor = Level::getBlockEntity(posA, player->getDimensionId());
    chestBlockActor->setCustomName("§b" + player->getName() + "§e的移动背包");
    chestBlockActor->refreshData();

    ContainerID nextContainerID = serverPlayer->_nextContainerCounter();

    std::shared_ptr<ContainerOpenPacket> packet = Utils::createPacket<ContainerOpenPacket>(MinecraftPacketIds::ContainerOpen);
    *(ContainerID *) ((uintptr_t) packet.get() + 48) = nextContainerID;
    *(ContainerType *) ((uintptr_t) packet.get() + 49) = ContainerType::CONTAINER;
    *(BlockPos *) ((uintptr_t) packet.get() + 52) = posA;
    *(ActorUniqueID *) ((uintptr_t) packet.get() + 64) = ActorUniqueID::INVALID_ID;

    this->inventoryMap[player->getNetworkIdentifier()->getHash()] = {nextContainerID, player->getActorUniqueId(), player->getDimensionId(), posA, posB};

    Schedule::delay([this, player, nextContainerID, packet] {
        Utils::sendPacket(player, packet);
        uint32_t page = mPocketInventory.playerInventoryPageMap[player->getActorUniqueId()];
        sendInventorySlots(player, nextContainerID, page * InventoryMaxSize, (page * InventoryMaxSize) + InventoryMaxSize);
    }, 3);
}

void PocketInventory::sendInventorySlots(Player *player, ContainerID id, uint32_t first, uint32_t last) {
    auto it = this->playerInventoryMap.find(player->getActorUniqueId());
    if (it != this->playerInventoryMap.end()) {
        auto &items = it->second;
        for (uint32_t i = first, slot = 0; i <= last; ++i, slot++) {
            auto item = items.find(i);
            if (item == items.end()) {
                sendInventorySlot(player, id, slot, ItemStack::EMPTY_ITEM);
            } else {
                sendInventorySlot(player, id, slot, item->second);
            }
        }
        uint32_t page = mPocketInventory.playerInventoryPageMap[player->getActorUniqueId()];
        ItemStack lastItem(VanillaItemNames::CherrySign, 1, 0, nullptr);
        lastItem.setCustomName("§o§l§e上一页");
        lastItem.setCustomLore({"§6点击前往背包上一页", "§e当前页数(" + to_string(page) + "/" + to_string(MaxPage) + ")"});
        ItemStack nextItem(VanillaItemNames::WarpedSign, 1, 0, nullptr);
        nextItem.setCustomName("§o§l§e下一页");
        nextItem.setCustomLore({"§6点击前往背包下一页", "§e当前页数(" + to_string(page) + "/" + to_string(MaxPage) + ")"});
        sendInventorySlot(player, id, LastPageSlot, lastItem);
        sendInventorySlot(player, id, NextPageSlot, nextItem);
    } else {
        player->sendText("还未拥有移动背包");
    }
}

void PocketInventory::sendInventorySlot(Player *player, ContainerID id, uint32_t slot, const ItemStack &item) {
    std::shared_ptr<InventorySlotPacket> slotPacket = Utils::createPacket<InventorySlotPacket>(MinecraftPacketIds::InventorySlot);
    *(ContainerID *) ((uintptr_t) slotPacket.get() + 48) = id;
    *(uint32_t *) ((uintptr_t) slotPacket.get() + 52) = slot;
    auto descriptor = (NetworkItemStackDescriptor *) ((uintptr_t) slotPacket.get() + 56);
    Utils::loadItem(descriptor, item);
    Utils::sendPacket(player, slotPacket);
}

void
PocketInventory::sendItemStackResponseSuccess(Player *player, TypedClientNetId<ItemStackRequestIdTag, int, 0> &netId, ContainerEnumName fromContainer, uint32_t fromSlot, const ItemStack &fromItem, ContainerEnumName toContainer, uint32_t toSlot, const ItemStack &toItem) {
    auto responsePacket = Utils::createPacket<ItemStackResponsePacket>(MinecraftPacketIds::ItemStackResponse);
    auto &infoList = dAccess<vector<ItemStackResponseInfo>, 48>(responsePacket.get());

    std::vector<ItemStackResponseContainerInfo> infos;
    std::vector<ItemStackResponseSlotInfo> fromSlots;
    fromSlots.emplace_back(fromSlot, fromSlot, fromItem.getCount(), get<TypedServerNetId<ItemStackNetIdTag, int, 0>>(fromItem.getItemStackNetIdVariant().id), string(), 0);
    infos.emplace_back(fromContainer, fromSlots);

    std::vector<ItemStackResponseSlotInfo> toSlots;
    toSlots.emplace_back(toSlot, toSlot, toItem.getCount(), get<TypedServerNetId<ItemStackNetIdTag, int, 0>>(toItem.getItemStackNetIdVariant().id), string(), 0);
    infos.emplace_back(toContainer, toSlots);

    infoList.emplace_back(ItemStackResponseInfo::Result::OK, netId, infos);
    Utils::sendPacket(player, responsePacket);
}

void
PocketInventory::sendItemStackResponseError(Player *player, TypedClientNetId<ItemStackRequestIdTag, int, 0> &netId) {
    auto responsePacket = Utils::createPacket<ItemStackResponsePacket>(MinecraftPacketIds::ItemStackResponse);
    auto &infoList = dAccess<vector<ItemStackResponseInfo>, 48>(responsePacket.get());
    infoList.emplace_back(ItemStackResponseInfo::Result::ERROR, netId, std::vector<ItemStackResponseContainerInfo>());
    Utils::sendPacket(player, responsePacket);
}

void PocketInventory::onContainerClosePacket(ContainerClosePacket &packet) {
    for (auto &it: this->inventoryMap) {
        ContainerID containerId = dAccess<ContainerID, 48>(&packet);
        if (containerId == get<0>(it.second)) {
            auto player = Global<Level>->getPlayer(get<1>(it.second));
            AutomaticID<Dimension, int> dim = get<2>(it.second);
            BlockPos pos1 = get<3>(it.second);
            BlockPos pos2 = get<4>(it.second);
            Level::setBlock(pos1, dim, BedrockBlockNames::Air, 0);
            Level::setBlock(pos2, dim, BedrockBlockNames::Air, 0);
            savePlayerData(player);
            break;
        }
    }
}

void PocketInventory::loadPlayerData(Player *player) {
    //logger.info("加载玩家数据 {}", player->getName());
    unordered_map<uint32_t, ItemStack> items;
    ifstream data("plugins/NativeEnhancements/players/" + player->getXuid() + ".dat", std::ios::binary);
    if (data.is_open()) {
        std::string bin((std::istreambuf_iterator<char>(data)), std::istreambuf_iterator<char>());
        data.close();
        if (!bin.empty()) {
            auto nbt = CompoundTag::fromSNBT(bin);
            if (nbt->contains("Inventory")) {
                auto inventory = nbt->getList("Inventory");
                for (auto &tag: *inventory) {
                    auto it = tag->asCompoundTag();
                    int slot = it->getInt("Slot");
                    auto item = it->getCompound("Item");
                    ItemStack *itemStack = ItemStack::create(item->clone());
                    items[slot] = *itemStack;
                }
            }
        }
    }
    this->playerInventoryMap[player->getActorUniqueId()] = items;
}

void PocketInventory::savePlayerData(Player *player) {
    //logger.info("保存玩家数据 {}", player->getName());
    auto it = this->playerInventoryMap.find(player->getActorUniqueId());
    if (it == this->playerInventoryMap.end()) {
        return;
    }
    ofstream data("plugins/NativeEnhancements/players/" + player->getXuid() + ".dat");
    if (data.is_open()) {
        auto nbt = Utils::makeTag<CompoundTag>(Tag::Type::Compound);
        auto inventory = Utils::makeTag<ListTag>(Tag::Type::List);
        for (auto &item: it->second) {
            auto tag = Utils::makeTag<CompoundTag>(Tag::Type::Compound);
            tag->putInt("Slot", item.first);
            tag->putCompound("Item", item.second.save());
            inventory->add(std::move(tag));
        }
        nbt->put("Inventory", std::move(inventory));
        data << nbt->toSNBT();
        data.close();
    }
}

void PocketInventory::updateInventoryItem(Player *player, uint32_t slot, const ItemStack &item) {
    uint32_t page = this->playerInventoryPageMap[player->getActorUniqueId()];
    uint32_t index = (page * InventoryMaxSize) + slot;
    this->playerInventoryMap[player->getActorUniqueId()][index] = item;
}

ItemStack &PocketInventory::getInventoryItem(Player *player, uint32_t slot) {
    uint32_t page = this->playerInventoryPageMap[player->getActorUniqueId()];
    uint32_t index = (page * InventoryMaxSize) + slot;
    return this->playerInventoryMap[player->getActorUniqueId()][index];
}

bool PocketInventory::getFakeChestPos(Player *player, BlockPos *posA, BlockPos *posB) {
    auto pos = player->getBlockPos();
    for (int i = 3; i <= 7; ++i) {
        posA->x = pos.x;
        posA->y = pos.y + i;
        posA->z = pos.z;

        posB->x = pos.x;
        posB->y = pos.y + i;
        posB->z = pos.z + 1;

        auto blockA = Level::getBlock(*posA, player->getDimensionId());
        auto blockB = Level::getBlock(*posB, player->getDimensionId());
        if (blockA->isAir() && blockB->isAir()) {
            return true;
        }
    }
    return false;
}

TInstanceHook(void, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVItemStackRequestPacket@@@Z", ServerNetworkHandler, NetworkIdentifier const &identifier, ItemStackRequestPacket const &packet) {
    mPocketInventory.logger.info("ItemStackRequest {}", packet.toString());
    auto it = mPocketInventory.inventoryMap.find(identifier.getHash());
    if (it != mPocketInventory.inventoryMap.end()) {
        mPocketInventory.logger.info("当前虚拟背包玩家信息 {} {}", (int) get<0>(it->second), get<1>(it->second));
        if (packet.batch == nullptr || packet.batch->list.empty()) {
            return;
        }
        auto player = Global<Level>->getPlayer(get<1>(it->second));
        auto &list = packet.batch->list;
        if (list.size() > 1) {
            AutomaticID<Dimension, int> dim = get<2>(it->second);
            BlockPos pos1 = get<3>(it->second);
            BlockPos pos2 = get<4>(it->second);
            Level::setBlock(pos1, dim, BedrockBlockNames::Air, 0);
            Level::setBlock(pos2, dim, BedrockBlockNames::Air, 0);

            std::shared_ptr<ContainerClosePacket> closePacket = Utils::createPacket<ContainerClosePacket>(MinecraftPacketIds::ContainerClose);
            *(ContainerID *) ((uintptr_t) closePacket.get() + 48) = get<0>(it->second);

            mPocketInventory.inventoryMap.erase(it);
            mPocketInventory.savePlayerData(player);

            Utils::sendPacket(player, closePacket);
            player->sendText("您不可以在移动背包里拆分");
            return;
        }
        for (auto &item: list) {
            for (auto &action: item->actionList) {
                switch (action->mType) {
                    case ItemStackRequestActionType::Take:
                    case ItemStackRequestActionType::Place: {
                        auto &op = (ItemStackRequestActionTransferBase &) *action;
                        if (op.srcInfo.mSlot == LastPageSlot) {
                            mPocketInventory.logger.info("上一页");
                            mPocketInventory.sendItemStackResponseError(player, item->netId);
                            uint32_t page = mPocketInventory.playerInventoryPageMap[player->getActorUniqueId()];
                            if (page != 0) {
                                page--;
                                mPocketInventory.playerInventoryPageMap[player->getActorUniqueId()] = page;
                                mPocketInventory.sendInventorySlots(player, get<0>(it->second), page * InventoryMaxSize, (page * InventoryMaxSize) + InventoryMaxSize);
                            } else {
                                mPocketInventory.logger.warn("已经到第一页了");
                            }
                            break;
                        }
                        if (op.srcInfo.mSlot == NextPageSlot) {
                            mPocketInventory.logger.info("下一页");
                            mPocketInventory.sendItemStackResponseError(player, item->netId);
                            uint32_t page = mPocketInventory.playerInventoryPageMap[player->getActorUniqueId()];
                            if (page < MaxPage) {
                                page++;
                                mPocketInventory.playerInventoryPageMap[player->getActorUniqueId()] = page;
                                mPocketInventory.sendInventorySlots(player, get<0>(it->second), page * InventoryMaxSize, (page * InventoryMaxSize) + InventoryMaxSize);
                            } else {
                                mPocketInventory.logger.warn("已经到最后一页了");
                            }
                            break;
                        }
                        if (op.srcInfo.mId == ContainerEnumName::LevelEntityContainer && op.dstInfo.mId == ContainerEnumName::CursorContainer) {
                            mPocketInventory.logger.info("从虚拟箱子拿到光标上");
                            ItemStack targetItem = mPocketInventory.getInventoryItem(player, op.srcInfo.mSlot);
                            ItemStack itemStack = player->getPlayerUIItem((PlayerUISlot) 0);

                            if (targetItem.hasUserData() && targetItem.getUserData()->contains("PocketInventory")){
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("不可将特殊物品放进背包");
                                break;
                            }

                            if (itemStack.isNull()) {
                                targetItem.serverInitNetId();
                                player->setPlayerUIItem((PlayerUISlot) 0, targetItem);
                                mPocketInventory.updateInventoryItem(player, op.srcInfo.mSlot, ItemStack::EMPTY_ITEM);
                                mPocketInventory.logger.info("光标为空，直接放东西 {}", op.dstInfo.mSlot);
                            } else if (itemStack.matchesItem(targetItem)) {
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    mPocketInventory.sendItemStackResponseError(player, item->netId);
                                    mPocketInventory.logger.error("当前物品数量异常 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                player->setPlayerUIItem((PlayerUISlot) 0, itemStack);
                                mPocketInventory.updateInventoryItem(player, op.srcInfo.mSlot, ItemStack::EMPTY_ITEM);
                                mPocketInventory.logger.info("光标有东西，类型一样，更新数量 {}", itemStack.getCount());
                            } else {
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("当前物品冲突 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                break;
                            }
                            //mPocketInventory.sendItemStackResponseSuccess(player,item->netId,op.srcInfo.mId,op.srcInfo.mSlot,targetItem,op.dstInfo.mId,op.dstInfo.mSlot,itemStack);
                        }
                        if ((op.srcInfo.mId == ContainerEnumName::HotbarContainer && op.dstInfo.mId == ContainerEnumName::CursorContainer) || (op.srcInfo.mId == ContainerEnumName::InventoryContainer && op.dstInfo.mId == ContainerEnumName::CursorContainer)) {
                            mPocketInventory.logger.info("把玩家背包物品放到光标");
                            ItemStack targetItem = player->getSupplies().getItem(op.srcInfo.mSlot, ContainerID::Inventory);
                            ItemStack itemStack = player->getPlayerUIItem((PlayerUISlot) 0);

                            if (targetItem.hasUserData() && targetItem.getUserData()->contains("PocketInventory")){
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("不可将特殊物品放进背包");
                                break;
                            }

                            if (itemStack.isNull()) {
                                player->setPlayerUIItem((PlayerUISlot) 0, targetItem);
                                player->getSupplies().setItem(op.srcInfo.mSlot, ItemStack::EMPTY_ITEM, ContainerID::Inventory, false);
                                mPocketInventory.logger.info("光标没有东西，直接放置");
                            } else if (itemStack.matchesItem(targetItem)) {
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    mPocketInventory.sendItemStackResponseError(player, item->netId);
                                    mPocketInventory.logger.error("当前物品数量异常 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                player->setPlayerUIItem((PlayerUISlot) 0, itemStack);
                                mPocketInventory.logger.info("光标有一样的东西，更新数量 {}", itemStack.getCount());
                            } else {
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("当前物品冲突 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                break;
                            }
                            mPocketInventory.sendItemStackResponseSuccess(player, item->netId, op.srcInfo.mId, op.srcInfo.mSlot, targetItem, op.dstInfo.mId, op.dstInfo.mSlot, itemStack);
                        }
                        // 直接移动到背包
                        if ((op.srcInfo.mId == ContainerEnumName::LevelEntityContainer && op.dstInfo.mId == ContainerEnumName::CombinedHotbarAndInventoryContainer) || (op.srcInfo.mId == ContainerEnumName::LevelEntityContainer && op.dstInfo.mId == ContainerEnumName::HotbarContainer) || (op.srcInfo.mId == ContainerEnumName::LevelEntityContainer && op.dstInfo.mId == ContainerEnumName::InventoryContainer)) {
                            mPocketInventory.logger.info("从虚拟背包移动到背包");
                            ItemStack targetItem = mPocketInventory.getInventoryItem(player, op.srcInfo.mSlot);
                            ItemStack itemStack = player->getSupplies().getItem(op.dstInfo.mSlot, ContainerID::Inventory);

                            if (targetItem.hasUserData() && targetItem.getUserData()->contains("PocketInventory")){
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("不可将特殊物品放进背包");
                                break;
                            }

                            if (itemStack.isNull()) {
                                targetItem.serverInitNetId();
                                player->getSupplies().setItem(op.dstInfo.mSlot, targetItem, ContainerID::Inventory, false);
                                mPocketInventory.updateInventoryItem(player, op.srcInfo.mSlot, ItemStack::EMPTY_ITEM);
                                mPocketInventory.logger.info("当前物品放置 {}", op.dstInfo.mSlot);
                            } else if (itemStack.matchesItem(targetItem)) {
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    mPocketInventory.sendItemStackResponseError(player, item->netId);
                                    mPocketInventory.logger.error("当前物品数量异常 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                player->getSupplies().setItem(op.dstInfo.mSlot, itemStack, ContainerID::Inventory, false);
                                mPocketInventory.updateInventoryItem(player, op.srcInfo.mSlot, ItemStack::EMPTY_ITEM);
                                mPocketInventory.logger.info("当前物品数量增加 {}", op.dstInfo.mSlot);
                            } else {
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("当前物品冲突 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                break;
                            }
                            //mPocketInventory.sendItemStackResponseSuccess(player,item->netId,op.srcInfo.mId,op.srcInfo.mSlot,targetItem,op.dstInfo.mId,op.dstInfo.mSlot,itemStack);
                        }
                        if ((op.srcInfo.mId == ContainerEnumName::HotbarContainer && op.dstInfo.mId == ContainerEnumName::LevelEntityContainer) || (op.srcInfo.mId == ContainerEnumName::InventoryContainer && op.dstInfo.mId == ContainerEnumName::LevelEntityContainer) || (op.srcInfo.mId == ContainerEnumName::CombinedHotbarAndInventoryContainer && op.dstInfo.mId == ContainerEnumName::LevelEntityContainer)) {
                            mPocketInventory.logger.info("从玩家背包移动到虚拟背包");
                            ItemStack targetItem = mPocketInventory.getInventoryItem(player, op.dstInfo.mSlot);
                            ItemStack itemStack = player->getSupplies().getItem(op.srcInfo.mSlot, ContainerID::Inventory);

                            if (targetItem.hasUserData() && targetItem.getUserData()->contains("PocketInventory")){
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("不可将特殊物品放进背包");
                                break;
                            }

                            if (targetItem.isNull()) {
                                player->getSupplies().setItem(op.srcInfo.mSlot, ItemStack::EMPTY_ITEM, ContainerID::Inventory, false);
                                mPocketInventory.updateInventoryItem(player, op.dstInfo.mSlot, itemStack);
                            } else if (targetItem.matchesItem(itemStack)) {
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    mPocketInventory.sendItemStackResponseError(player, item->netId);
                                    mPocketInventory.logger.error("当前物品数量异常 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                player->getSupplies().setItem(op.srcInfo.mSlot, ItemStack::EMPTY_ITEM, ContainerID::Inventory, false);
                                mPocketInventory.updateInventoryItem(player, op.dstInfo.mSlot, itemStack);
                                mPocketInventory.logger.info("当前物品匹配，数量增加 {}", itemStack.getCount());
                            } else {
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("当前物品冲突 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                break;
                            }
                            mPocketInventory.sendItemStackResponseSuccess(player, item->netId, op.srcInfo.mId, op.srcInfo.mSlot, targetItem, op.dstInfo.mId, op.dstInfo.mSlot, itemStack);
                        }
                        if ((op.srcInfo.mId == ContainerEnumName::CursorContainer && op.dstInfo.mId == ContainerEnumName::HotbarContainer) || (op.srcInfo.mId == ContainerEnumName::CursorContainer && op.dstInfo.mId == ContainerEnumName::InventoryContainer)) {
                            mPocketInventory.logger.info("从光标移动到玩家背包");
                            ItemStack targetItem = player->getPlayerUIItem((PlayerUISlot) 0);
                            ItemStack itemStack = player->getSupplies().getItem(op.dstInfo.mSlot, ContainerID::Inventory);

                            if (itemStack.isNull()) {
                                player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                                player->getSupplies().setItem(op.dstInfo.mSlot, targetItem, ContainerID::Inventory, false);
                                mPocketInventory.logger.info("快捷栏为空，直接放置 {}", targetItem.toString());
                            } else if (itemStack.matchesItem(targetItem)) {
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    mPocketInventory.sendItemStackResponseError(player, item->netId);
                                    mPocketInventory.logger.error("当前物品数量异常 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                                player->getSupplies().setItem(op.dstInfo.mSlot, itemStack, ContainerID::Inventory, false);
                                mPocketInventory.logger.info("快捷栏已有物品，而且相同，合并后数量 {}", itemStack.getCount());
                            } else {
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("当前物品冲突 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                break;
                            }
                            mPocketInventory.sendItemStackResponseSuccess(player, item->netId, op.srcInfo.mId, op.srcInfo.mSlot, targetItem, op.dstInfo.mId, op.dstInfo.mSlot, itemStack);
                        }
                        if (op.srcInfo.mId == ContainerEnumName::CursorContainer && op.dstInfo.mId == ContainerEnumName::LevelEntityContainer) {
                            mPocketInventory.logger.info("把光标和虚拟背包的物品合并");
                            ItemStack targetItem = mPocketInventory.getInventoryItem(player, op.dstInfo.mSlot);
                            ItemStack itemStack = player->getPlayerUIItem((PlayerUISlot) 0);

                            if (targetItem.hasUserData() && targetItem.getUserData()->contains("PocketInventory")){
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("不可将特殊物品放进背包");
                                break;
                            }

                            if (targetItem.isNull()) {
                                mPocketInventory.updateInventoryItem(player, op.dstInfo.mSlot, itemStack);
                                mPocketInventory.sendInventorySlot(player, get<0>(it->second), op.dstInfo.mSlot, itemStack);
                                player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                            } else if (targetItem.matchesItem(targetItem)) {
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    mPocketInventory.sendItemStackResponseError(player, item->netId);
                                    mPocketInventory.logger.error("当前物品数量异常 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                mPocketInventory.updateInventoryItem(player, op.dstInfo.mSlot, itemStack);
                                mPocketInventory.sendInventorySlot(player, get<0>(it->second), op.dstInfo.mSlot, itemStack);
                                player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                            } else {
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                mPocketInventory.logger.error("当前物品冲突 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                            }
                            mPocketInventory.sendItemStackResponseSuccess(player, item->netId, op.srcInfo.mId, op.srcInfo.mSlot, targetItem, op.dstInfo.mId, op.dstInfo.mSlot, itemStack);
                        }
                        if (op.srcInfo.mId == ContainerEnumName::LevelEntityContainer && op.dstInfo.mId == ContainerEnumName::LevelEntityContainer) {
                            mPocketInventory.logger.info("虚拟背包物品拆分");
                            ItemStack targetItem = mPocketInventory.getInventoryItem(player, op.srcInfo.mSlot);
                            targetItem.serverInitNetId();
                            ItemStack itemStack(targetItem);
                            itemStack.serverInitNetId();

                            int count = targetItem.getCount() / 2;
                            targetItem.set(count);
                            itemStack.set(count);
                            mPocketInventory.updateInventoryItem(player, op.srcInfo.mSlot, targetItem);
                            mPocketInventory.updateInventoryItem(player, op.dstInfo.mSlot, itemStack);

                            mPocketInventory.sendInventorySlot(player, get<0>(it->second), op.srcInfo.mSlot, targetItem);
                            mPocketInventory.sendInventorySlot(player, get<0>(it->second), op.dstInfo.mSlot, itemStack);

                            if (targetItem.getCount() % 2 != 0) {
                                int value = targetItem.getCount() % 2;
                                ItemStack uiItem(targetItem);
                                uiItem.set(value);
                                player->setPlayerUIItem((PlayerUISlot) 0, uiItem);
                            }
                            mPocketInventory.sendItemStackResponseSuccess(player, item->netId, op.srcInfo.mId, op.srcInfo.mSlot, targetItem, op.dstInfo.mId, op.dstInfo.mSlot, itemStack);
                        }
                        if ((op.srcInfo.mId == ContainerEnumName::HotbarContainer && op.dstInfo.mId == ContainerEnumName::HotbarContainer) || (op.srcInfo.mId == ContainerEnumName::InventoryContainer && op.dstInfo.mId == ContainerEnumName::InventoryContainer) || (op.srcInfo.mId == ContainerEnumName::CombinedHotbarAndInventoryContainer && op.dstInfo.mId == ContainerEnumName::CombinedHotbarAndInventoryContainer)) {
                            if (op.srcInfo.mType.hasServerNetId()) {
                                mPocketInventory.logger.info("玩家背包物品交换");
                                ItemStack targetItem = player->getSupplies().getItem(op.srcInfo.mSlot, ContainerID::Inventory);
                                ItemStack itemStack = player->getSupplies().getItem(op.dstInfo.mSlot, ContainerID::Inventory);
                                if (targetItem.matchesItem(itemStack)) {
                                    if (targetItem.getCount() + itemStack.getCount() > targetItem.getMaxStackSize()) {
                                        mPocketInventory.sendItemStackResponseError(player, item->netId);
                                        mPocketInventory.logger.error("当前物品数量异常 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                        break;
                                    }
                                    itemStack.add(targetItem.getCount());
                                    targetItem = ItemStack::EMPTY_ITEM;
                                    player->getSupplies().setItem(op.srcInfo.mSlot, targetItem, ContainerID::Inventory, false);
                                    player->getSupplies().setItem(op.dstInfo.mSlot, itemStack, ContainerID::Inventory, false);
                                    mPocketInventory.logger.info("物品一样，合并后数量 {}", itemStack.getCount());
                                } else {
                                    player->getSupplies().swapSlots(op.srcInfo.mSlot, op.dstInfo.mSlot);
                                    mPocketInventory.logger.info("交换物品位置");
                                }
                                mPocketInventory.sendItemStackResponseSuccess(player, item->netId, op.srcInfo.mId, op.srcInfo.mSlot, targetItem, op.dstInfo.mId, op.dstInfo.mSlot, itemStack);
                            } else {
                                mPocketInventory.logger.info("背包物品拆分");
                                ItemStack targetItem = player->getSupplies().getItem(op.srcInfo.mSlot, ContainerID::Inventory);
                                ItemStack itemStack(targetItem);
                                itemStack.serverInitNetId();

                                int count = targetItem.getCount() / 2;
                                targetItem.set(count);
                                itemStack.set(count);
                                player->getSupplies().setItem(op.srcInfo.mSlot, targetItem, ContainerID::Inventory, false);
                                player->getSupplies().setItem(op.dstInfo.mSlot, itemStack, ContainerID::Inventory, false);
                                if (targetItem.getCount() % 2 != 0) {
                                    int value = targetItem.getCount() % 2;
                                    ItemStack uiItem(targetItem);
                                    uiItem.set(value);
                                    player->setPlayerUIItem((PlayerUISlot) 0, uiItem);
                                }
                                mPocketInventory.sendItemStackResponseSuccess(player, item->netId, op.srcInfo.mId, op.srcInfo.mSlot, targetItem, op.dstInfo.mId, op.dstInfo.mSlot, itemStack);
                            }
                        }
                        break;
                    }
                    case ItemStackRequestActionType::Swap: {
                        auto &op = (ItemStackRequestActionTransferBase &) *action;
                        if (op.src && op.dst) {
                            mPocketInventory.sendItemStackResponseError(player, item->netId);
                            mPocketInventory.logger.warn("携带版暂时不支持操作");
                            break;
                        } else {
                            if (op.dstInfo.mSlot == LastPageSlot || op.dstInfo.mSlot == NextPageSlot) {
                                mPocketInventory.logger.error("不可跟特殊物品交互");
                                mPocketInventory.sendItemStackResponseError(player, item->netId);
                                break;
                            }
                            if (op.dstInfo.mId == ContainerEnumName::HotbarContainer || op.dstInfo.mId == ContainerEnumName::InventoryContainer || op.dstInfo.mId == ContainerEnumName::CombinedHotbarAndInventoryContainer) {
                                mPocketInventory.logger.info("玩家背包物品交换");
                                ItemStack targetItem = player->getPlayerUIItem((PlayerUISlot) 0);
                                if (targetItem.isNull()) {
                                    mPocketInventory.sendItemStackResponseError(player, item->netId);
                                    mPocketInventory.logger.error("不支持移动端物品交换");
                                    break;
                                }
                                ItemStack itemStack = player->getSupplies().getItem(op.dstInfo.mSlot, ContainerID::Inventory);
                                if (itemStack.matchesItem(targetItem)) {
                                    if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                        mPocketInventory.sendItemStackResponseError(player, item->netId);
                                        mPocketInventory.logger.error("当前物品数量异常 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                        break;
                                    }
                                    targetItem.add(itemStack.getCount());
                                    player->getSupplies().setItem(op.dstInfo.mSlot, targetItem, ContainerID::Inventory, true);
                                    player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                                } else {
                                    player->setPlayerUIItem((PlayerUISlot) 0, itemStack);
                                    player->getSupplies().setItem(op.dstInfo.mSlot, targetItem, ContainerID::Inventory, true);
                                }
                                mPocketInventory.sendItemStackResponseSuccess(player, item->netId, ContainerEnumName::CursorContainer, 0, ItemStack::EMPTY_ITEM, op.dstInfo.mId, op.dstInfo.mSlot, targetItem);
                            }
                            if (op.dstInfo.mId == ContainerEnumName::LevelEntityContainer) {
                                mPocketInventory.logger.info("虚拟背包物品交换");
                                ItemStack targetItem = player->getPlayerUIItem((PlayerUISlot) 0);

                                if (targetItem.hasUserData() && targetItem.getUserData()->contains("PocketInventory")){
                                    mPocketInventory.sendItemStackResponseError(player, item->netId);
                                    mPocketInventory.logger.error("不可将特殊物品放进背包");
                                    break;
                                }

                                if (targetItem.isNull()) {
                                    mPocketInventory.sendItemStackResponseError(player, item->netId);
                                    mPocketInventory.logger.error("不支持移动端物品交换");
                                    break;
                                }
                                ItemStack itemStack = mPocketInventory.getInventoryItem(player, op.dstInfo.mSlot);
                                if (itemStack.matchesItem(targetItem)) {
                                    if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                        mPocketInventory.sendItemStackResponseError(player, item->netId);
                                        mPocketInventory.logger.error("当前物品数量异常 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                        break;
                                    }
                                    targetItem.add(itemStack.getCount());
                                    mPocketInventory.updateInventoryItem(player, op.dstInfo.mSlot, targetItem);
                                    player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                                    mPocketInventory.logger.info("物品一样，合并后数量 {}", targetItem);
                                } else {
                                    player->setPlayerUIItem((PlayerUISlot) 0, itemStack);
                                    mPocketInventory.updateInventoryItem(player, op.dstInfo.mSlot, targetItem);
                                    mPocketInventory.logger.info("物品不一样，交换");
                                }
                                mPocketInventory.sendItemStackResponseSuccess(player, item->netId, ContainerEnumName::CursorContainer, 0, ItemStack::EMPTY_ITEM, op.dstInfo.mId, op.dstInfo.mSlot, targetItem);
                            }
                        }
                        break;
                    }
                    case ItemStackRequestActionType::Drop: {
                        auto &op = (ItemStackRequestActionTransferBase &) *action;
                        ItemStack targetItem;
                        if (op.srcInfo.mId == ContainerEnumName::CursorContainer) {
                            targetItem = player->getPlayerUIItem((PlayerUISlot) 0);
                            player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                        } else if (op.srcInfo.mId == ContainerEnumName::HotbarContainer || op.srcInfo.mId == ContainerEnumName::InventoryContainer || op.srcInfo.mId == ContainerEnumName::CombinedHotbarAndInventoryContainer) {
                            targetItem = player->getSupplies().getItem(op.srcInfo.mSlot, ContainerID::Inventory);
                            player->getSupplies().setItem(op.srcInfo.mSlot, ItemStack::EMPTY_ITEM, ContainerID::Inventory, false);
                        } else if (op.srcInfo.mId == ContainerEnumName::LevelEntityContainer) {
                            targetItem = mPocketInventory.getInventoryItem(player, op.srcInfo.mSlot);
                            mPocketInventory.updateInventoryItem(player, op.srcInfo.mSlot, ItemStack::EMPTY_ITEM);
                        } else {
                            mPocketInventory.sendItemStackResponseError(player, item->netId);
                            mPocketInventory.logger.error("当前操作容器不支持");
                            break;
                        }
                        auto itemActor = player->spawnAtLocation(targetItem, -0.5);
                        auto &rot = player->getRotation();

                        auto x = (float) (-sin(rot.y / 180 * M_PI) * 0.3);
                        auto z = (float) (cos(rot.y / 180 * M_PI) * 0.3);

                        itemActor->setVelocity({x, 0.0f, z});

                        auto responsePacket = Utils::createPacket<ItemStackResponsePacket>(MinecraftPacketIds::ItemStackResponse);
                        auto &infoList = dAccess<vector<ItemStackResponseInfo>, 48>(responsePacket.get());

                        std::vector<ItemStackResponseContainerInfo> infos;
                        std::vector<ItemStackResponseSlotInfo> fromSlots;
                        fromSlots.emplace_back(0, 0, 0, get<TypedServerNetId<ItemStackNetIdTag, int, 0>>(targetItem.getItemStackNetIdVariant().id), string(), 0);
                        infos.emplace_back(op.srcInfo.mId, fromSlots);

                        infoList.emplace_back(ItemStackResponseInfo::Result::OK, item->netId, infos);
                        Utils::sendPacket(player, responsePacket);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        mPocketInventory.sendInventorySlot(player, ContainerID::PlayerUIOnly, 0, player->getPlayerUIItem((PlayerUISlot) 0));
        player->refreshInventory();
        return;
    }
    original(this, identifier, packet);
}

TInstanceHook(void, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVContainerClosePacket@@@Z", ServerNetworkHandler, NetworkIdentifier const &identifier, ContainerClosePacket const &packet) {
    auto it = mPocketInventory.inventoryMap.find(identifier.getHash());
    if (it != mPocketInventory.inventoryMap.end()) {
        //mPocketInventory.logger.info("当前虚拟背包玩家信息 {} {}", (int) get<0>(it->second), get<1>(it->second));
        auto player = Global<Level>->getPlayer(get<1>(it->second));
        AutomaticID<Dimension, int> dim = get<2>(it->second);
        BlockPos pos1 = get<3>(it->second);
        BlockPos pos2 = get<4>(it->second);
        Level::setBlock(pos1, dim, BedrockBlockNames::Air, 0);
        Level::setBlock(pos2, dim, BedrockBlockNames::Air, 0);
        mPocketInventory.inventoryMap.erase(it);
        mPocketInventory.savePlayerData(player);
    }
    original(this, identifier, packet);
}


TStaticHook(void, "?addLooseCreativeItems@Item@@SAX_NAEBVBaseGameVersion@@VItemRegistryRef@@@Z", Item, bool value, BaseGameVersion const &version, ItemRegistryRef itemRegistry) {
    original(value, version, itemRegistry);
    ItemStack itemStack(VanillaItemNames::Saddle, 1, 0, nullptr);
    itemStack.setCustomName("§o§l§b移动背包");
    itemStack.setCustomLore({"§e右键或者长按打开背包"});
    itemStack.getUserData()->putBoolean("PocketInventory", true);
    Utils::enchant(itemStack, EnchantType::durability, MINSHORT);
    ItemLockHelper::setKeepOnDeath(itemStack, true);
    Item::addCreativeItem(itemRegistry, itemStack);
}

/*
TInstanceHook(void, "?onBlockChanged@Dimension@@UEAAXAEAVBlockSource@@AEBVBlockPos@@IAEBVBlock@@2HPEBUActorBlockSyncMessage@@W4BlockChangedEventTarget@@PEAVActor@@@Z",
              Dimension, BlockSource &source, BlockPos const &pos, uint32_t value1,
              Block const &block1, Block const &block2, int value2, ActorBlockSyncMessage const *message, BlockChangedEventTarget target, Actor *actor) {
    mPocketInventory.logger.info("onBlockChanged {} {} {} {} {}",pos.toString(),value1,value2,block1.getName(),block2.getName());
    original(this, source, pos,value1,block1,block2,value2,message,target,actor);
}*/
