#pragma once

#include <vector>
#include <memory>
class Role;
class Damage
{

public:
    Damage(Role *from, int num, Role *target, int type);
    static std::vector<std::unique_ptr<Damage>> damageVec;
    Role *from;
    Role *target;
    int num;
    int type;

    static void tick();
    static void to(Role *from, Role *target, int num, int type = 0);
};