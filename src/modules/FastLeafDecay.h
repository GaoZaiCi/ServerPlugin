//
// Created by ASUS on 2023/11/19.
//

#ifndef NATIVEENHANCEMENTS_FASTLEAFDECAY_H
#define NATIVEENHANCEMENTS_FASTLEAFDECAY_H

#include "EventAPI.h"
#include "LoggerAPI.h"

#include "mc/HashedString.hpp"
#include "mc/VanillaBlockTypeIds.hpp"

using namespace std;
using namespace Event;

class FastLeafDecay {
public:
    Logger logger;
    unordered_set<string> LeafBlocks{
            VanillaBlockTypeIds::Leaves,
            VanillaBlockTypeIds::Leaves2,
            VanillaBlockTypeIds::AzaleaLeaves,
            VanillaBlockTypeIds::AzaleaLeavesFlowered,
            VanillaBlockTypeIds::MangroveLeaves,
            VanillaBlockTypeIds::CherryLeaves
    };
    unordered_map<BlockPos,int> leafBlockPos;
public:
    FastLeafDecay();

    void init();

    void onTick(ServerLevel *level);
};


#endif //NATIVEENHANCEMENTS_FASTLEAFDECAY_H
