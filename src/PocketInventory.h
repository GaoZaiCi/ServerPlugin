//
// Created by ASUS on 2023/11/13.
//

#ifndef NATIVEENHANCEMENTS_POCKETINVENTORY_H
#define NATIVEENHANCEMENTS_POCKETINVENTORY_H

#include <tuple>

#include "Utils.h"
#include <LoggerAPI.h>
#include "mc/Packet.hpp"
#include "mc/ItemStackRequestAction.hpp"
#include "mc/ItemStackNetIdVariant.hpp"
#include "mc/ItemStackRequestSlotInfo.hpp"
#include "mc/BlockPos.hpp"

using namespace std;

class ItemStack;

class InventorySlotPacket;

class Dimension;


namespace std {
    template<>
    struct hash<ActorUniqueID> {
        size_t operator()(const ActorUniqueID &id) const {
            return hash<long long>()(id);
        }
    };
}

class PocketInventory {
private:
    Logger logger;
public:
    unordered_map<ActorUniqueID, unordered_map<uint32_t, ItemStack>> playerInventoryMap;
    unordered_map<unsigned __int64, tuple<ContainerID, ActorUniqueID, AutomaticID<Dimension, int>, BlockPos, BlockPos>> inventoryMap;
public:
    PocketInventory();

    void init();

    void sendInventorySlot(Player *player, ContainerID id, uint32_t slot, const ItemStack &item);

    void onContainerClosePacket(ContainerClosePacket &packet);
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

class ItemStackRequestActionPlace : public ItemStackRequestActionTransferBase {
};

class ItemStackRequestActionTake : public ItemStackRequestActionTransferBase {
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
            switch (it->mType) {
                case ItemStackRequestActionType::Take: {
                    auto p = (ItemStackRequestActionTake *) it.get();
                    ss << p->toString() << " ";
                    break;
                }
                case ItemStackRequestActionType::Place: {
                    auto p = (ItemStackRequestActionPlace *) it.get();
                    ss << p->toString() << " ";
                    break;
                }
                default:
                    ss << ItemStackRequestAction::getActionTypeName(it->mType) << " ";
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

#endif //NATIVEENHANCEMENTS_POCKETINVENTORY_H
