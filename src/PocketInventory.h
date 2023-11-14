//
// Created by ASUS on 2023/11/13.
//

#ifndef NATIVEENHANCEMENTS_POCKETINVENTORY_H
#define NATIVEENHANCEMENTS_POCKETINVENTORY_H

#include <LoggerAPI.h>

class ItemStack;
class InventorySlotPacket;

class PocketInventory {
private:
    Logger logger;
public:
    PocketInventory();

    void init();

    class InventorySlotPacketProxy {
    public:
        InventorySlotPacketProxy();
        ContainerID getContainerID() const;
        void setContainerID(ContainerID id);
        uint32_t getSlot() const;
        void setSlot(uint32_t slot);
        ItemStack getItem(Level &level) const;
        void setItem(const ItemStack &item);
    };
};


#endif //NATIVEENHANCEMENTS_POCKETINVENTORY_H
