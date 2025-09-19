#include "Damage.h"
#include "Role.h"
std::vector<std::unique_ptr<Damage>> Damage::damageVec;

Damage::Damage(Role *from, int num, Role *target, int type)
    : from(from), num(num), target(target), type(type) {}
void Damage::tick()
{
    for (auto &damage : damageVec)
    {
        if (damage && damage->target)
        {
            damage->target->hurt(damage.get());
        }
    }
    damageVec.clear();
}

void Damage::to(Role *from, Role *target, int num, int type)
{
    damageVec.push_back(std::make_unique<Damage>(from, num, target, type));
}