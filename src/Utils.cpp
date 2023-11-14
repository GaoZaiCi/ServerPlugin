//
// Created by ASUS on 2023/10/25.
//

#include "Utils.h"
#include "mc/ListTag.hpp"
#include "mc/CompoundTag.hpp"
#include "mc/Actor.hpp"
#include "mc/ItemStack.hpp"
#include "mc/LoopbackPacketSender.hpp"
#include "mc/Level.hpp"
#include "mc/Player.hpp"

bool Utils::enchant(ListTag &tag, EnchantType type, short level, bool compulsion) {
    if (!compulsion) {
        for (Tag *t: tag) {
            if (t->getTagType() == Tag::Type::Compound) {
                CompoundTag *compoundTag = t->asCompoundTag();
                if (compoundTag->contains("id")) {
                    short id = compoundTag->getShort("id");
                    if (id == type) {
                        return false;
                    }
                }
            }
        }
    }
    std::unique_ptr<CompoundTag> item = makeTag<CompoundTag>(Tag::Type::Compound);
    item->putShort("id", type);
    item->putShort("lvl", level);
    tag.add(std::move(item));
    return true;
}

void Utils::enchant(ItemStack &itemStack, EnchantType type, short level, bool compulsion) {
    if (itemStack.hasUserData()) {
        CompoundTag *compoundTag = itemStack.getUserData();
        if (compoundTag->contains(ItemStack::TAG_ENCHANTS)) {
            ListTag *listTag = compoundTag->getList(ItemStack::TAG_ENCHANTS);
            enchant(*listTag, type, level, compulsion);
        } else {
            std::unique_ptr<ListTag> listTag = makeTag<ListTag>(Tag::Type::List);
            enchant(*listTag, type, level, compulsion);
            compoundTag->put(ItemStack::TAG_ENCHANTS, std::move(listTag));
        }
    } else {
        std::unique_ptr<CompoundTag> nbt = makeTag<CompoundTag>(Tag::Type::Compound);
        std::unique_ptr<ListTag> listTag = makeTag<ListTag>(Tag::Type::List);
        enchant(*listTag, type, level, compulsion);
        nbt->put(ItemStack::TAG_ENCHANTS, std::move(listTag));
        itemStack.setUserData(std::move(nbt));
    }
}

AttributeInstance &Utils::getEntityAttribute(Actor &actor, const Attribute &attribute) {
    auto &instance = actor.getAttribute(attribute);
    return (AttributeInstance &) instance;
}

void Utils::sendPacket(Player *player, const std::shared_ptr<Packet> &packet) {
    auto sender = (LoopbackPacketSender *) Global<Level>->getPacketSender();
    sender->sendToClient(*player->getNetworkIdentifier(), *packet, (SubClientId) player->getClientSubId());
}

std::string Utils::getContainerEnumName(ContainerEnumName name) {
    std::unordered_map<ContainerEnumName, std::string, ContainerEnumNameHasher> &map = *(std::unordered_map<ContainerEnumName, std::string, ContainerEnumNameHasher> *) dlsym_real("?ContainerCollectionNameMap@@3V?$unordered_map@W4ContainerEnumName@@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@UContainerEnumNameHasher@@U?$equal_to@W4ContainerEnumName@@@3@V?$allocator@U?$pair@$$CBW4ContainerEnumName@@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@std@@@3@@std@@A");
    return map[name];
}

void Utils::loadItem(NetworkItemStackDescriptor *descriptor, const ItemStack &item) {
    auto fun = (void (*)(NetworkItemStackDescriptor *, ItemStack const &)) dlsym_real("??0NetworkItemStackDescriptor@@QEAA@AEBVItemStack@@@Z");
    fun(descriptor, item);
}

