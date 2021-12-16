// This Header is auto generated by BDSLiteLoader Toolchain
#pragma once
#define AUTO_GENERATED
#include "../Global.h"

#define BEFORE_EXTRA
// Include Headers or Declare Types Here

#undef BEFORE_EXTRA

class BlockStateCommandParam {

#define AFTER_EXTRA
// Add Member There

#undef AFTER_EXTRA

#ifndef DISABLE_CONSTRUCTOR_PREVENTION_BLOCKSTATECOMMANDPARAM
public:
    class BlockStateCommandParam& operator=(class BlockStateCommandParam const&) = delete;
    BlockStateCommandParam(class BlockStateCommandParam const&) = delete;
    BlockStateCommandParam() = delete;
#endif

public:
    MCAPI BlockStateCommandParam(std::string, std::string, enum BlockStateCommandParam::Type);
    MCAPI bool setBlockState(class Block const* *, class CommandOutput&) const;
    MCAPI ~BlockStateCommandParam();

protected:

private:

};