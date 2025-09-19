#pragma once
#include <unordered_map>
#include "PropType.h"

class PropModel
{
private:
    // 基础属性模板（静态常量）
    static const std::unordered_map<PropType, double>& baseProps()
    {
        static const std::unordered_map<PropType, double> props = {
            {PropType::MAX_HP, 100},
            {PropType::HP, 100},
            {PropType::ATK, 10},
            {PropType::DEF, 10},
            {PropType::JUMP_SPEED, 200}
        };
        return props;
    }

public:
    // 创建角色属性（可覆盖基础属性）
    static std::unordered_map<PropType, double> roleProps(
        const std::unordered_map<PropType, double>& overrides = {})
    {
        auto props = baseProps();  // 获取基础属性副本
        
        // 应用覆盖属性
        for (const auto& [key, value] : overrides) {
            props[key] = value;
        }
        
        return props;
    }
    
    // 创建战士属性（带默认覆盖）
    static std::unordered_map<PropType, double> warriorProps()
    {
        return roleProps({
            {PropType::MAX_HP, 200},
            {PropType::HP, 200},
            {PropType::ATK, 15}
        });
    }
};