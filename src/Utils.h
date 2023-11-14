//
// Created by ASUS on 2023/10/25.
//

#ifndef TEMPLATEPLUGIN_UTILS_H
#define TEMPLATEPLUGIN_UTILS_H


#include "mc/Tag.hpp"
#include "mc/AttributeInstance.hpp"
#include "mc/MinecraftPackets.hpp"

enum EnchantType : short {
    arrowDamage = 19,//力量
    arrowFire = 21,//火矢
    arrowInfinite = 22,//无限
    arrowKnockback = 20,//冲击
    crossbowMultishot = 33,//多重箭
    crossbowPiercing = 34,//穿透
    crossbowQuickCharge = 35,//快速装填
    curse_binding = 0,//绑定诅咒
    curse_vanishing = 0,//消失诅咒
    damage_all = 9,//锋利
    damage_arthropods = 11,//节肢克星
    damage_undead = 10,//亡灵克星
    digging = 15,//效率
    durability = 17,//耐久
    fire = 13,//火焰附加
    fishingSpeed = 24,//饵钓
    frostwalker = 25,//冰霜行者
    knockback = 12,//击退
    lootBonus = 14,//抢夺
    lootBonusDigger = 18,//时运
    lootBonusFishing = 23,//海之眷顾
    mending = 26,//经验修补
    oxygen = 6,//水下呼吸
    protect_all = 0,//保护
    protect_explosion = 3,//爆炸保护
    protect_fall = 2,//摔落保护
    protect_fire = 1,//火焰保护
    protect_projectile = 4,//弹射物保护
    thorns = 5,//荆棘
    untouching = 16,//精准采集
    waterWalker = 7,//深海探索者
    waterWorker = 8,//水下速掘
    tridentChanneling = 32,//引雷
    tridentLoyalty = 31,//忠诚
    tridentRiptide = 30,//激流
    tridentImpaling = 29,//穿刺
};

class NetworkItemStackDescriptor;

class Utils {
public:
    template<class T>
    static std::unique_ptr<T> makeTag(Tag::Type type) {
        std::unique_ptr<Tag> t = Tag::newTag(type);
        std::unique_ptr<T> tag = (std::unique_ptr<T> &&) t;
        return tag;
    }

    static bool enchant(ListTag &tag, EnchantType type, short level, bool compulsion = false);

    static void enchant(ItemStack &itemStack, EnchantType type, short level, bool compulsion = false);

    static AttributeInstance &getEntityAttribute(Actor &actor, const Attribute &);

    template<class T>
    static std::shared_ptr<T> createPacket(MinecraftPacketIds id) {
        std::shared_ptr<Packet> packet = MinecraftPackets::createPacket(id);
        return (std::shared_ptr<T> &&) packet;
    }

    static void sendPacket(Player *player, const std::shared_ptr<Packet> &packet);

    static std::string getContainerEnumName(ContainerEnumName name);

    static void loadItem(NetworkItemStackDescriptor *descriptor,const ItemStack &item);

    struct ContainerEnumNameHasher {
    public:
        std::size_t operator()(ContainerEnumName k) const {
            return 0x9FFAAC085635BC91 * ((unsigned __int8) k ^ 0xCBF29CE484222325);
        }
    };

};


#endif //TEMPLATEPLUGIN_UTILS_H
