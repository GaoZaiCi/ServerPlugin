//
// Created by ASUS on 2023/11/13.
//

#ifndef NATIVEENHANCEMENTS_POCKETINVENTORY_H
#define NATIVEENHANCEMENTS_POCKETINVENTORY_H

#include <tuple>

#include "Utils.h"
#include "LoggerAPI.h"
#include "mc/Packet.hpp"
#include "mc/ItemStackRequestAction.hpp"
#include "mc/ItemStackNetIdVariant.hpp"
#include "mc/ItemStackRequestSlotInfo.hpp"
#include "mc/BlockPos.hpp"
#include "mc/CompoundTag.hpp"
#include "mc/NetworkItemStackDescriptor.hpp"
#include "mc/HashedString.hpp"

using namespace std;

class ItemStack;

class ItemInstance;

class InventorySlotPacket;

class UpdateBlockPacket;

class Dimension;

#define InventoryMaxSize 52
#define LastPageSlot 52
#define NextPageSlot 53
#define MaxPage 10


namespace std {
    template<>
    struct hash<ActorUniqueID> {
        size_t operator()(const ActorUniqueID &id) const {
            return hash<long long>()(id);
        }
    };
}

class PocketInventory {
public:
    Logger logger;
    unordered_map<ActorUniqueID, unordered_map<uint32_t, ItemStack>> playerInventoryMap;
    unordered_map<ActorUniqueID, uint32_t> playerInventoryPageMap;
    unordered_map<uint64_t, tuple<ContainerID, ActorUniqueID, AutomaticID<Dimension, int>, BlockPos, BlockPos>> inventoryMap;
    unordered_map<HashedString, uint32_t> blockRuntimeIds;
public:
    PocketInventory();

    void init();

    void openInventory(Player *player);

    void sendInventorySlots(Player *player, ContainerID id, uint32_t first, uint32_t last);

    void sendInventorySlot(Player *player, ContainerID id, uint32_t slot, const ItemStack &item);

    void
    sendItemStackResponseSuccess(Player *player, TypedClientNetId<ItemStackRequestIdTag, int, 0> &netId, ContainerEnumName fromContainer, uint32_t fromSlot, const ItemStack &fromItem, ContainerEnumName toContainer, uint32_t toSlot, const ItemStack &toItem);

    void sendItemStackResponseError(Player *player, TypedClientNetId<ItemStackRequestIdTag, int, 0> &netId);

    void onContainerClosePacket(ContainerClosePacket &packet);

    bool onUpdateBlockPacket(UpdateBlockPacket &packet, uint64_t id);

    void loadPlayerData(Player *player);

    void savePlayerData(Player *player);

    void updateInventoryItem(Player *player, uint32_t slot, const ItemStack &item);

    ItemStack &getInventoryItem(Player *player, uint32_t slot);

    bool getFakeChestPos(Player *player, BlockPos *posA, BlockPos *posB);
};


class ItemStackRequestActionTransferBase : public ItemStackRequestAction {
public:
    bool dst = false, src = false;
    uint8_t amount = 0;
    ItemStackRequestSlotInfo srcInfo;
    ItemStackRequestSlotInfo dstInfo;
public:
    virtual ~ItemStackRequestActionTransferBase() = default;

    inline std::string toString() const {
        std::stringstream ss;
        if (src) {
            ss << "src:[" << Utils::getContainerEnumName(srcInfo.mId) << "(" << (int) srcInfo.mId << ")" << " " << (int) srcInfo.mSlot << " " << srcInfo.mType.toString() << "]";
        }
        if (dst) {
            ss << "dst:[" << Utils::getContainerEnumName(dstInfo.mId) << "(" << (int) dstInfo.mId << ")" << " " << (int) dstInfo.mSlot << " " << dstInfo.mType.toString() << "]";
        }
        ss << " amount:[" << (int) amount << "]";
        return ss.str();
    }
};

class ItemStackRequestData {
public:
    TypedClientNetId<ItemStackRequestIdTag, int, 0> netId;
    std::vector<std::string> filterList;
    int filterStringCause;
    std::vector<std::unique_ptr<ItemStackRequestAction>> actionList;
public:
    inline string toString() {
        stringstream ss;
        ss << "netId:[" << netId.netId << "] ";
        ss << "filter:[";
        for (auto &it: filterList) {
            ss << it << " ";
        }
        ss << "] ";
        ss << "action:[";
        for (auto &it: actionList) {
            ss << ItemStackRequestAction::getActionTypeName(it->mType) << " ";
            switch (it->mType) {
                case ItemStackRequestActionType::Place:
                case ItemStackRequestActionType::Swap:
                case ItemStackRequestActionType::Drop:
                case ItemStackRequestActionType::Take: {
                    auto p = (ItemStackRequestActionTransferBase *) it.get();
                    ss << p->toString() << " ";
                    break;
                }
                default:
                    break;
            }
        }
        ss << "] ";
        ss << "filterStringCause:[" << filterStringCause << "] ";
        return ss.str();
    }
};

class ItemStackRequestBatch {
public:
    std::vector<std::unique_ptr<ItemStackRequestData>> list;
public:
    inline string toString() {
        stringstream ss;
        ss << "list:[";
        for (auto &it: list) {
            ss << it->toString() << " ";
        }
        ss << "]";
        return ss.str();
    }
};

class ItemStackRequestPacket : public Packet {
public:
    std::unique_ptr<ItemStackRequestBatch> batch;
public:
    inline string toString() const {
        stringstream ss;
        ss << "batch:[" << batch->toString() << "]";
        return ss.str();
    }
};

class BlockActorDataPacket : public Packet {
public:
    BlockPos mPos;
    CompoundTag mNbt;
};

class ContainerOpenPacket : public Packet {
public:
    ContainerID mContainerID;
    ContainerType mContainerType;
    BlockPos mPos;
    ActorUniqueID mActorUniqueID;
};

class InventorySlotPacket : public Packet {
public:
    ContainerID mContainerID;
    uint32_t mSlot;
    NetworkItemStackDescriptor mNetworkItemStackDescriptor;
};

class UpdateBlockPacket : public Packet {
public:
    BlockPos mPos;
    uint32_t layer;
    uint8_t flags;
    uint32_t mRuntimeId;

    string toString() const {
        stringstream ss;
        ss << "mPos:[" << mPos.toString() << "] ";
        ss << "layer:[" << layer << "] ";
        ss << "flags:[" << (int) flags << "] ";
        ss << "mRuntimeId:[" << mRuntimeId << "]";
        return ss.str();
    }
};

class ContainerClosePacket : public Packet {
public:
    ContainerID mContainerID;
};

class UpdateSubChunkBlocksPacket : public Packet {
public:
    class NetworkBlockInfo {
    public:
        BlockPos mPos;
        uint32_t mRuntimeId;
        uint8_t flags;
        ActorUniqueID syncedUpdateActorUniqueId;
        uint32_t syncedUpdateType;

        NetworkBlockInfo(const BlockPos &mPos, uint32_t mRuntimeId, uint8_t flags)
                : mPos(mPos), mRuntimeId(mRuntimeId), flags(flags), syncedUpdateActorUniqueId(ActorUniqueID::INVALID_ID), syncedUpdateType(0) {}

        string toString() const {
            stringstream ss;
            ss << "pos:" << mPos.toString() << " ";
            ss << "flags:" << (int) flags << " ";
            ss << "mRuntimeId:" << mRuntimeId << " ";
            ss << "syncedUpdateActorUniqueId:" << syncedUpdateActorUniqueId << " ";
            ss << "syncedUpdateType:" << syncedUpdateType;
            return ss.str();
        }
    };

    vector<NetworkBlockInfo> updates;
    vector<NetworkBlockInfo> list;
};


class ItemStackResponseSlotInfo {
public:
    uint8_t slot;
    uint8_t hotbarSlot;
    uint8_t count;
    TypedServerNetId<ItemStackNetIdTag, int, 0> itemStackId;
    std::string customName;
    uint16_t durabilityCorrection;

    ItemStackResponseSlotInfo(uint8_t slot, uint8_t hotbarSlot, uint8_t count, const TypedServerNetId<ItemStackNetIdTag, int, 0> &itemStackId, const string &customName, uint16_t durabilityCorrection)
            : slot(slot), hotbarSlot(hotbarSlot), count(count), itemStackId(itemStackId), customName(customName), durabilityCorrection(durabilityCorrection) {}

public:
    string toString() {
        stringstream ss;
        ss << "slot: " << (int) slot << " hotbarSlot: " << (int) hotbarSlot << " count: " << (int) count << " ";
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
    ItemStackResponseContainerInfo(ContainerEnumName containerId, const vector<ItemStackResponseSlotInfo> &slots)
            : containerId(containerId), slots(slots) {}

    string toString() {
        std::stringstream ss;
        ss << "container:" << Utils::getContainerEnumName(containerId) << endl;
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

    ItemStackResponseInfo(Result result, const TypedClientNetId<ItemStackRequestIdTag, int, 0> &netId, const vector<ItemStackResponseContainerInfo> &responseContainerInfoList)
            : result(result), netId(netId), responseContainerInfoList(responseContainerInfoList) {}

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

class ItemStackResponsePacket : public Packet {
public:
    vector<ItemStackResponseInfo> infos;
};


#endif //NATIVEENHANCEMENTS_POCKETINVENTORY_H
