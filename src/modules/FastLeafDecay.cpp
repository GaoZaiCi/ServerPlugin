//
// Created by ASUS on 2023/11/19.
//

#include "FastLeafDecay.h"

#include "mc/LeavesBlock.hpp"
#include "mc/Level.hpp"
#include "mc/Random.hpp"
#include "mc/BlockSource.hpp"
#include "mc/Block.hpp"

extern FastLeafDecay mFastLeafDecay;

FastLeafDecay::FastLeafDecay() : logger(__FILE__) {
}

void FastLeafDecay::init() {

}

void FastLeafDecay::onTick(ServerLevel *level) {
    if (this->leafBlockPos.empty()) {
        return;
    }
    for (auto it = leafBlockPos.begin(); it != leafBlockPos.end();) {
        auto block = Level::getBlockEx(it->first, it->second);
        if (!block || block->isAir()) {
            it = leafBlockPos.erase(it);
        } else {
            if (LeafBlocks.count(block->getName())) {
                auto &leavesBlock = (LeavesBlock &) *(block->getLegacyBlock()).createWeakPtr();
                auto source = Level::getBlockSource(it->second);
                leavesBlock.LeavesBlock::randomTick(*source, it->first, Random::getThreadLocal());
                ++it;
            } else {
                it = leafBlockPos.erase(it);
            }
        }
    }
}

// LeavesBlock::onRemove(BlockSource &,BlockPos const &)
TInstanceHook(void, "?onRemove@LeavesBlock@@UEBAXAEAVBlockSource@@AEBVBlockPos@@@Z", LeavesBlock, BlockSource &source, BlockPos const &pos) {
    try {
        for (uint8_t i = 0; i < 6; i++) {
            BlockPos blockPos = pos.neighbor(i);
            auto &block = source.getBlock(blockPos);
            if (mFastLeafDecay.LeafBlocks.count(block.getName())) {
                mFastLeafDecay.leafBlockPos.emplace(blockPos,source.getDimensionId());
            }
        }
    } catch (...) {
        mFastLeafDecay.logger.error("LeavesBlock::onRemove");
    }
    return original(this, source, pos);
}

// LogBlock::onRemove(BlockSource &,BlockPos const &)
TInstanceHook(void, "?onRemove@LogBlock@@UEBAXAEAVBlockSource@@AEBVBlockPos@@@Z", LeavesBlock, BlockSource &source, BlockPos const &pos) {
    try {
        for (uint8_t i = 0; i < 6; i++) {
            BlockPos blockPos = pos.neighbor(i);
            auto &block = source.getBlock(blockPos);
            if (mFastLeafDecay.LeafBlocks.count(block.getName())) {
                mFastLeafDecay.leafBlockPos.emplace(blockPos,source.getDimensionId());
            }
        }
    } catch (...) {
        mFastLeafDecay.logger.error("LogBlock::onRemove");
    }
    return original(this, source, pos);
}
