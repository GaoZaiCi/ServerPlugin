//
// Created by ASUS on 2023/11/17.
//

#ifndef NATIVEENHANCEMENTS_BLOODMOON_H
#define NATIVEENHANCEMENTS_BLOODMOON_H

#include <EventAPI.h>
#include <LoggerAPI.h>

using namespace std;
using namespace Event;

class BloodMoon {
public:
    Logger logger;
    bool enabled = false;
    vector<string> texts = {"§e你怎么睡得着的？", "§e你这个年龄段，你睡得着觉？", "§e有僵尸偷你东西！", "§e什么？你想睡觉？"};
public:
    BloodMoon();

    void init();

    void onTick(ServerLevel *level);
};


#endif //NATIVEENHANCEMENTS_BLOODMOON_H
