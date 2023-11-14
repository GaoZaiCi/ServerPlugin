//
// Created by ASUS on 2023/11/13.
//

#include "PocketInventory.h"

#include <EventAPI.h>
#include <ScheduleAPI.h>
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
#include "mc/CompoundTag.hpp"
#include "mc/ServerPlayer.hpp"
#include "mc/Block.hpp"
#include "mc/HashedString.hpp"
#include "mc/ItemStackResponsePacket.hpp"
#include "mc/PlayerInventory.hpp"
#include "mc/BedrockBlockNames.hpp"
#include "mc/BlockActor.hpp"

using nlohmann::json;
using namespace Event;

extern PocketInventory mPocketInventory;

PocketInventory::PocketInventory() : logger(__FILE__) {
}

void PocketInventory::init() {
    PlayerJoinEvent::subscribe_ref([this](auto &event) {
        ItemStack itemStack("minecraft:enchanted_golden_apple", 1, 0, nullptr);
        unordered_map<uint32_t, ItemStack> items;
        for (int i = 0; i < 9 * 6; ++i) {
            items[i] = itemStack;
        }
        this->playerInventoryMap[event.mPlayer->getActorUniqueId()] = items;
        return true;
    });
    PlayerLeftEvent::subscribe_ref([this](auto &event) {
        this->playerInventoryMap.erase(event.mPlayer->getActorUniqueId());
        return true;
    });
    PlayerUseItemEvent::subscribe_ref([this](auto &event) {
        logger.info("打开界面 {}", event.mItemStack->getNbt()->toSNBT());
        auto player = (ServerPlayer *) event.mPlayer;

        BlockPos pos1 = player->getBlockPos();
        pos1.y += 3;

        BlockPos pos2 = pos1;
        pos2.z++;

        Level::setBlock(pos1, player->getDimensionId(), VanillaBlockTypeIds::Chest, 4);
        Level::setBlock(pos2, player->getDimensionId(), VanillaBlockTypeIds::Chest, 4);

        BlockActor *chestBlockActor = Level::getBlockEntity(pos1, player->getDimensionId());
        chestBlockActor->setCustomName("§b" + player->getName() + "§e的移动背包");
        chestBlockActor->refreshData();

        ContainerID nextContainerID = player->_nextContainerCounter();

        std::shared_ptr<ContainerOpenPacket> packet = Utils::createPacket<ContainerOpenPacket>(MinecraftPacketIds::ContainerOpen);
        *(ContainerID *) ((uintptr_t) packet.get() + 48) = nextContainerID;
        *(ContainerType *) ((uintptr_t) packet.get() + 49) = ContainerType::CONTAINER;
        *(BlockPos *) ((uintptr_t) packet.get() + 52) = pos1;
        *(ActorUniqueID *) ((uintptr_t) packet.get() + 64) = ActorUniqueID::INVALID_ID;

        Schedule::delay([this, player, nextContainerID, packet, pos1, pos2] {
            Utils::sendPacket(player, packet);
            auto it = this->playerInventoryMap.find(player->getActorUniqueId());
            if (it != this->playerInventoryMap.end()) {
                int slot = 0;
                for (auto &item: it->second) {
                    sendInventorySlot(player, nextContainerID, slot++, item.second);
                }
            } else {
                player->sendText("还未拥有移动背包");
            }
            this->inventoryMap[player->getNetworkIdentifier()->getHash()] = {nextContainerID, player->getActorUniqueId(), player->getDimensionId(), pos1, pos2};
        }, 10);

        return true;
    });

}

void PocketInventory::sendInventorySlot(Player *player, ContainerID id, uint32_t slot, const ItemStack &item) {
    std::shared_ptr<InventorySlotPacket> slotPacket = Utils::createPacket<InventorySlotPacket>(MinecraftPacketIds::InventorySlot);
    *(ContainerID *) ((uintptr_t) slotPacket.get() + 48) = id;
    *(uint32_t *) ((uintptr_t) slotPacket.get() + 52) = slot;
    NetworkItemStackDescriptor *descriptor = (NetworkItemStackDescriptor *) ((uintptr_t) slotPacket.get() + 56);
    Utils::loadItem(descriptor, item);
    Utils::sendPacket(player, slotPacket);
}

void PocketInventory::onContainerClosePacket(ContainerClosePacket &packet) {
    for(auto &it : this->inventoryMap){
        ContainerID containerId = dAccess<ContainerID, 48>(&packet);
        if (containerId == get<0>(it.second)){
            AutomaticID<Dimension, int> dim = get<2>(it.second);
            BlockPos pos1 = get<3>(it.second);
            BlockPos pos2 = get<4>(it.second);
            Global<Level>->setBlock(pos1, dim, BedrockBlockNames::Air, 0);
            Global<Level>->setBlock(pos2, dim, BedrockBlockNames::Air, 0);
            break;
        }
    }
}

TInstanceHook(void, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVItemStackRequestPacket@@@Z", ServerNetworkHandler, NetworkIdentifier const &identifier, ItemStackRequestPacket const &packet) {
    Logger logger(__FILE__);
    logger.info("ItemStackRequest {}", packet.toString());
    auto it = mPocketInventory.inventoryMap.find(identifier.getHash());
    if (it != mPocketInventory.inventoryMap.end()) {
        logger.info("当前虚拟背包玩家信息 {} {}", (int) get<0>(it->second), get<1>(it->second));

        if (packet.batch == nullptr || packet.batch->list.empty()) {
            return;
        }

        auto player = Global<Level>->getPlayer(get<1>(it->second));

        auto &list = packet.batch->list;
        if (list.size() > 1){
            AutomaticID<Dimension, int> dim = get<2>(it->second);
            BlockPos pos1 = get<3>(it->second);
            BlockPos pos2 = get<4>(it->second);
            Global<Level>->setBlock(pos1, dim, BedrockBlockNames::Air, 0);
            Global<Level>->setBlock(pos2, dim, BedrockBlockNames::Air, 0);

            std::shared_ptr<ContainerClosePacket> closePacket = Utils::createPacket<ContainerClosePacket>(MinecraftPacketIds::ContainerClose);
            *(ContainerID *) ((uintptr_t) closePacket.get() + 48) = get<0>(it->second);

            mPocketInventory.inventoryMap.erase(it);

            Utils::sendPacket(player, closePacket);
            player->sendText("您不可以在移动背包里拆分");
            return;
        }
        for (auto &item: list) {
            for (auto &action: item->actionList) {
                switch (action->mType) {
                    // 鼠标选中
                    case ItemStackRequestActionType::Take:
                    case ItemStackRequestActionType::Place: {
                        auto &op = (ItemStackRequestActionTransferBase &) *action;
                        auto &items = mPocketInventory.playerInventoryMap[player->getActorUniqueId()];
                        // 拿到鼠标上
                        if (op.srcInfo.mId == ContainerEnumName::LevelEntityContainer && op.dstInfo.mId == ContainerEnumName::CursorContainer) {
                            logger.info("从虚拟箱子拿到光标上");
                            ItemStack targetItem = items[op.srcInfo.mSlot];
                            ItemStack itemStack = player->getPlayerUIItem((PlayerUISlot) 0);

                            if (itemStack.isNull()){
                                targetItem.serverInitNetId();
                                player->setPlayerUIItem((PlayerUISlot) 0, targetItem);
                                logger.info("光标为空，直接放东西 {}", op.dstInfo.mSlot);
                            } else if (itemStack.matchesItem(targetItem)){
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    logger.error("当前物品数量异常 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                player->setPlayerUIItem((PlayerUISlot) 0, itemStack);
                                logger.info("光标有东西，类型一样，更新数量 {}", itemStack.getCount());
                            } else {
                                logger.error("当前物品冲突 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                break;
                            }
                            items[op.srcInfo.mSlot] = ItemStack::EMPTY_ITEM;
                        }
                        if ((op.srcInfo.mId == ContainerEnumName::HotbarContainer && op.dstInfo.mId == ContainerEnumName::CursorContainer) || (op.srcInfo.mId == ContainerEnumName::InventoryContainer && op.dstInfo.mId == ContainerEnumName::CursorContainer)) {
                            logger.info("把玩家背包物品放到光标");
                            ItemStack targetItem = player->getSupplies().getItem(op.srcInfo.mSlot, ContainerID::Inventory);
                            ItemStack itemStack = player->getPlayerUIItem((PlayerUISlot) 0);
                            if (itemStack.isNull()) {
                                player->setPlayerUIItem((PlayerUISlot) 0, targetItem);
                                player->getSupplies().setItem(op.srcInfo.mSlot, ItemStack::EMPTY_ITEM, ContainerID::Inventory, false);
                                logger.info("光标没有东西，直接放置");
                            } else if (itemStack.matchesItem(targetItem)) {
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    logger.error("当前物品数量异常 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                player->setPlayerUIItem((PlayerUISlot) 0, itemStack);
                                logger.info("光标有一样的东西，更新数量 {}",itemStack.getCount());
                            } else {
                                logger.error("当前物品冲突 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                break;
                            }
                        }
                        // 直接移动到背包
                        if (op.srcInfo.mId == ContainerEnumName::LevelEntityContainer && op.dstInfo.mId == ContainerEnumName::CombinedHotbarAndInventoryContainer) {
                            logger.info("从虚拟背包移动到背包");
                            ItemStack targetItem = items[op.srcInfo.mSlot];
                            ItemStack itemStack = player->getSupplies().getItem(op.dstInfo.mSlot, ContainerID::Inventory);

                            if (itemStack.isNull()){
                                targetItem.serverInitNetId();
                                player->getSupplies().setItem(op.dstInfo.mSlot, targetItem, ContainerID::Inventory, false);
                                logger.info("当前物品放置 {}", op.dstInfo.mSlot);
                            } else if (itemStack.matchesItem(targetItem)){
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    logger.error("当前物品数量异常 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                player->getSupplies().setItem(op.dstInfo.mSlot, itemStack, ContainerID::Inventory, false);
                                logger.info("当前物品数量增加 {}", op.dstInfo.mSlot);
                            } else {
                                logger.error("当前物品冲突 {} {} {}", op.dstInfo.mSlot, itemStack.toString(), targetItem.toString());
                                break;
                            }

                            auto responsePacket = Utils::createPacket<ItemStackResponsePacket>(MinecraftPacketIds::ItemStackResponse);
                            auto &infoList = dAccess<vector<ItemStackResponseInfo>, 48>(responsePacket.get());

                            std::vector<ItemStackResponseContainerInfo> infos;
                            std::vector<ItemStackResponseSlotInfo> slots;

                            slots.emplace_back(op.dstInfo.mSlot, op.dstInfo.mSlot, itemStack.getCount(), get<TypedServerNetId<ItemStackNetIdTag, int, 0>>(itemStack.getItemStackNetIdVariant().id), string(), 0);
                            infos.emplace_back(ContainerEnumName::CombinedHotbarAndInventoryContainer, slots);
                            infoList.emplace_back(ItemStackResponseInfo::Result::OK, item->netId, infos);
                            Utils::sendPacket(player, responsePacket);

                            mPocketInventory.sendInventorySlot(player, get<0>(it->second), op.srcInfo.mSlot, ItemStack::EMPTY_ITEM);

                            items[op.srcInfo.mSlot] = ItemStack::EMPTY_ITEM;
                        }
                        if ((op.srcInfo.mId == ContainerEnumName::HotbarContainer && op.dstInfo.mId == ContainerEnumName::LevelEntityContainer) || (op.srcInfo.mId == ContainerEnumName::InventoryContainer && op.dstInfo.mId == ContainerEnumName::LevelEntityContainer)) {
                            logger.info("从玩家背包移动到虚拟背包");
                            ItemStack targetItem = items[op.dstInfo.mSlot];
                            ItemStack itemStack = player->getSupplies().getItem(op.srcInfo.mSlot, ContainerID::Inventory);

                            if (targetItem.isNull()){
                                player->getSupplies().setItem(op.srcInfo.mSlot, ItemStack::EMPTY_ITEM, ContainerID::Inventory, false);
                            } else if (targetItem.matchesItem(itemStack)){
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    logger.error("当前物品数量异常 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                player->getSupplies().setItem(op.srcInfo.mSlot, ItemStack::EMPTY_ITEM, ContainerID::Inventory, false);
                                logger.info("当前物品匹配，数量增加 {}", itemStack.getCount());
                            } else {
                                logger.error("当前物品冲突 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                break;
                            }

                            auto responsePacket = Utils::createPacket<ItemStackResponsePacket>(MinecraftPacketIds::ItemStackResponse);
                            auto &infoList = dAccess<vector<ItemStackResponseInfo>, 48>(responsePacket.get());

                            std::vector<ItemStackResponseContainerInfo> infos;
                            std::vector<ItemStackResponseSlotInfo> slots;

                            slots.emplace_back(op.srcInfo.mSlot, op.srcInfo.mSlot, itemStack.getCount(), get<TypedServerNetId<ItemStackNetIdTag, int, 0>>(itemStack.getItemStackNetIdVariant().id), string(), 0);
                            infos.emplace_back(ContainerEnumName::CombinedHotbarAndInventoryContainer, slots);
                            infoList.emplace_back(ItemStackResponseInfo::Result::OK, item->netId, infos);
                            Utils::sendPacket(player, responsePacket);

                            mPocketInventory.sendInventorySlot(player, get<0>(it->second), op.dstInfo.mSlot, itemStack);

                            items[op.dstInfo.mSlot] = itemStack;
                        }
                        if ((op.srcInfo.mId == ContainerEnumName::CursorContainer && op.dstInfo.mId == ContainerEnumName::HotbarContainer) || (op.srcInfo.mId == ContainerEnumName::CursorContainer && op.dstInfo.mId == ContainerEnumName::InventoryContainer)) {
                            logger.info("从光标移动到玩家背包");
                            ItemStack targetItem = player->getPlayerUIItem((PlayerUISlot) 0);
                            ItemStack itemStack = player->getSupplies().getItem(op.dstInfo.mSlot, ContainerID::Inventory);
                            if (itemStack.isNull()) {
                                player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                                player->getSupplies().setItem(op.dstInfo.mSlot, targetItem, ContainerID::Inventory, false);
                                logger.info("快捷栏为空，直接放置 {}", targetItem.toString());
                            } else if (itemStack.matchesItem(targetItem)) {
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    logger.error("当前物品数量异常 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                                player->getSupplies().setItem(op.dstInfo.mSlot, itemStack, ContainerID::Inventory, false);
                                logger.info("快捷栏已有物品，而且相同，合并后数量 {}", itemStack.getCount());
                            } else {
                                logger.error("当前物品冲突 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                break;
                            }
                        }
                        if (op.srcInfo.mId == ContainerEnumName::CursorContainer && op.dstInfo.mId == ContainerEnumName::LevelEntityContainer) {
                            logger.info("把光标和虚拟背包的物品合并");
                            ItemStack targetItem = items[op.dstInfo.mSlot];
                            ItemStack itemStack = player->getPlayerUIItem((PlayerUISlot) 0);
                            if (targetItem.isNull()) {
                                items[op.dstInfo.mSlot] = itemStack;
                                mPocketInventory.sendInventorySlot(player, get<0>(it->second), op.dstInfo.mSlot, itemStack);
                                player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                            } else if (targetItem.matchesItem(targetItem)) {
                                if (itemStack.getCount() + targetItem.getCount() > itemStack.getMaxStackSize()) {
                                    logger.error("当前物品数量异常 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                                    break;
                                }
                                itemStack.add(targetItem.getCount());
                                items[op.dstInfo.mSlot] = itemStack;
                                mPocketInventory.sendInventorySlot(player, get<0>(it->second), op.dstInfo.mSlot, itemStack);
                                player->setPlayerUIItem((PlayerUISlot) 0, ItemStack::EMPTY_ITEM);
                            } else {
                                logger.error("当前物品冲突 {} {} {}", op.srcInfo.mSlot, itemStack.toString(), targetItem.toString());
                            }
                        }
                        if (op.srcInfo.mId == ContainerEnumName::LevelEntityContainer && op.dstInfo.mId == ContainerEnumName::LevelEntityContainer) {
                            logger.info("虚拟背包物品拆分");
                            ItemStack targetItem = items[op.srcInfo.mSlot];
                            targetItem.serverInitNetId();
                            ItemStack itemItem(targetItem);
                            itemItem.serverInitNetId();

                            int count = targetItem.getCount() / 2;
                            targetItem.set(count);
                            itemItem.set(count);
                            items[op.srcInfo.mSlot] = targetItem;
                            items[op.dstInfo.mSlot] = itemItem;

                            mPocketInventory.sendInventorySlot(player, get<0>(it->second), op.srcInfo.mSlot, targetItem);
                            mPocketInventory.sendInventorySlot(player, get<0>(it->second), op.dstInfo.mSlot, itemItem);

                            if (targetItem.getCount() % 2 != 0) {
                                int value = targetItem.getCount() % 2;
                                ItemStack uiItem(targetItem);
                                uiItem.set(value);
                                player->setPlayerUIItem((PlayerUISlot) 0, uiItem);
                            }
                        }
                        if ((op.srcInfo.mId == ContainerEnumName::HotbarContainer && op.dstInfo.mId == ContainerEnumName::HotbarContainer) || (op.srcInfo.mId == ContainerEnumName::InventoryContainer && op.dstInfo.mId == ContainerEnumName::InventoryContainer)) {
                            logger.info("背包物品拆分");
                            ItemStack targetItem = player->getSupplies().getItem(op.srcInfo.mSlot, ContainerID::Inventory);
                            ItemStack itemItem(targetItem);
                            itemItem.serverInitNetId();

                            int count = targetItem.getCount() / 2;
                            targetItem.set(count);
                            itemItem.set(count);
                            player->getSupplies().setItem(op.srcInfo.mSlot, targetItem, ContainerID::Inventory, false);
                            player->getSupplies().setItem(op.dstInfo.mSlot, itemItem, ContainerID::Inventory, false);
                            if (targetItem.getCount() % 2 != 0) {
                                int value = targetItem.getCount() % 2;
                                ItemStack uiItem(targetItem);
                                uiItem.set(value);
                                player->setPlayerUIItem((PlayerUISlot) 0, uiItem);
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
                logger.warn("ItemStackRequestAction {}", ItemStackRequestAction::getActionTypeName(action->mType));
            }
        }
        mPocketInventory.sendInventorySlot(player, ContainerID::PlayerUIOnly, 0, player->getPlayerUIItem((PlayerUISlot) 0));
        player->refreshInventory();
        return;
    }
    original(this, identifier, packet);
}

TInstanceHook(void, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVContainerClosePacket@@@Z", ServerNetworkHandler, NetworkIdentifier const &identifier, ContainerClosePacket const &packet) {
    Logger logger(__FILE__);
    ContainerID containerId = dAccess<ContainerID, 48>(&packet);
    bool b = dAccess<bool, 49>(&packet);
    logger.info("ContainerClose {} {}", (int) containerId, b);
    auto it = mPocketInventory.inventoryMap.find(identifier.getHash());
    if (it != mPocketInventory.inventoryMap.end()) {
        logger.info("当前虚拟背包玩家信息 {} {}", (int) get<0>(it->second), get<1>(it->second));

        AutomaticID<Dimension, int> dim = get<2>(it->second);
        BlockPos pos1 = get<3>(it->second);
        BlockPos pos2 = get<4>(it->second);
        Global<Level>->setBlock(pos1, dim, BedrockBlockNames::Air, 0);
        Global<Level>->setBlock(pos2, dim, BedrockBlockNames::Air, 0);

        mPocketInventory.inventoryMap.erase(it);
    }
    original(this, identifier, packet);
}

