//
// Created by ASUS on 2023/11/17.
//

#ifndef NATIVEENHANCEMENTS_SUPERMOB_H
#define NATIVEENHANCEMENTS_SUPERMOB_H

#include <EventAPI.h>
#include <LoggerAPI.h>

using namespace std;
using namespace Event;

class SuperMob {
public:
    Logger logger;
public:
    SuperMob();

    void init();
};


#endif //NATIVEENHANCEMENTS_SUPERMOB_H
