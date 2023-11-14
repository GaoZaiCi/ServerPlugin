//
// Created by ASUS on 2023/11/13.
//

#include "PocketInventory.h"
#include "Utils.h"

#include <EventAPI.h>
#include <ScheduleAPI.h>
#include "mc/VanillaBlockTypeIds.hpp"
#include "mc/LoopbackPacketSender.hpp"
#include "mc/ContainerOpenPacket.hpp"
#include "mc/InventorySlotPacket.hpp"
#include "mc/NetworkItemStackDescriptor.hpp"
#include "mc/Level.hpp"
#include "mc/BlockPos.hpp"
#include "mc/ActorUniqueID.hpp"
#include "mc/ItemStack.hpp"
#include "mc/CompoundTag.hpp"
#include "mc/ServerPlayer.hpp"
#include "mc/Block.hpp"
#include "mc/HashedString.hpp"

PocketInventory::PocketInventory() : logger(__FILE__)  {
}

void PocketInventory::init() {
    Event::PlayerUseItemEvent::subscribe_ref([this](auto &event) {
        logger.info("打开界面 {}",event.mItemStack->getNbt()->toSNBT());
        auto player = (ServerPlayer *) event.mPlayer;

        BlockPos pos = player->getBlockPos();
        pos.y +=2;

        string nbt = R"({"Block":{"name":"minecraft:chest","states":{},"version":18095666},"Count":1b,"Damage":0s,"Name":"minecraft:chest","WasPickedUp":0b,"tag":{"Items":[{"Count":2b,"Damage":0s,"Name":"minecraft:gunpowder","Slot":0b,"WasPickedUp":0b},{"Count":4b,"Damage":0s,"Name":"minecraft:arrow","Slot":1b,"WasPickedUp":0b}]}})";

        auto tag = CompoundTag::fromSNBT(nbt);


        Level::setBlock(pos, player->getDimensionId(), VanillaBlockTypeIds::Chest, 4);
        Level::setBlock({pos.x,pos.y,pos.z+1}, player->getDimensionId(), VanillaBlockTypeIds::Chest, 4);

        //ChestBlockActor *chestBlockActor = (ChestBlockActor*)Level::getBlockEntity(pos,player->getDimensionId());
        //chestBlockActor->loadItems(*CompoundTag::fromSNBT(R"({"Items":[{"Count":2b,"Damage":0s,"Name":"minecraft:gunpowder","Slot":0b,"WasPickedUp":0b},{"Count":4b,"Damage":0s,"Name":"minecraft:arrow","Slot":1b,"WasPickedUp":0b}]})").release(),player->getLevel());
        //chestBlockActor->refreshData();

        std::shared_ptr<ContainerOpenPacket> packet = Utils::createPacket<ContainerOpenPacket>(MinecraftPacketIds::ContainerOpen);
        *(ContainerID *) ((uintptr_t) packet.get() + 48) = player->_nextContainerCounter();
        *(ContainerType *) ((uintptr_t) packet.get() + 49) = ContainerType::CONTAINER;
        *(BlockPos *) ((uintptr_t) packet.get() + 52) = pos;
        *(ActorUniqueID *) ((uintptr_t) packet.get() + 64) = ActorUniqueID::INVALID_ID;

        Schedule::delay([player,packet]{
            Utils::sendPacket(player,packet);

            ItemStack itemStack("minecraft:enchanted_golden_apple", 1, 0, nullptr);

            std::shared_ptr<InventorySlotPacket> packet2 = Utils::createPacket<InventorySlotPacket>(MinecraftPacketIds::InventorySlot);
            *(ContainerID *) ((uintptr_t) packet2.get() + 48) = dAccess<ContainerID, 48>(packet.get());
            *(uint32_t *) ((uintptr_t) packet2.get() + 52) = 0;
            NetworkItemStackDescriptor *d = (NetworkItemStackDescriptor *) ((uintptr_t) packet2.get() + 56);
            auto fun = (void (*)(NetworkItemStackDescriptor *, ItemStack const &)) dlsym_real("??0NetworkItemStackDescriptor@@QEAA@AEBVItemStack@@@Z");
            fun(d, itemStack);

            for (int i = 1; i < 40; ++i) {
                Utils::sendPacket(player,packet2);
                *(uint32_t *) ((uintptr_t) packet2.get() + 52) = i;
            }

        }, 20);

        return true;
    });

}

PocketInventory::InventorySlotPacketProxy::InventorySlotPacketProxy() {

}

ContainerID PocketInventory::InventorySlotPacketProxy::getContainerID() const {
    return ContainerID::Offhand;
}

void PocketInventory::InventorySlotPacketProxy::setContainerID(ContainerID id) {

}

uint32_t PocketInventory::InventorySlotPacketProxy::getSlot() const {
    return 0;
}

void PocketInventory::InventorySlotPacketProxy::setSlot(uint32_t slot) {

}

ItemStack PocketInventory::InventorySlotPacketProxy::getItem(Level &level) const {
    return ItemStack();
}

void PocketInventory::InventorySlotPacketProxy::setItem(const ItemStack &item) {

}

