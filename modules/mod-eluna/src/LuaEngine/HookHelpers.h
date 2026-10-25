/*
 * Copyright (C) 2010 - 2016 Eluna Lua Engine <http://emudevs.com/>
 * This program is free software licensed under GPL version 3
 * Please see the included DOCS/LICENSE.md for more information
 */

#ifndef _HOOK_HELPERS_H
#define _HOOK_HELPERS_H

#include "LuaEngine.h"
#include "ElunaUtility.h"

/*
 * Sets up the stack so that event handlers can be called.
 *
 * Returns the number of functions that were pushed onto the stack.
 */
template<typename K1, typename K2>
int Eluna::SetupStack(BindingMap<K1>* bindings1, BindingMap<K2>* bindings2, const K1& key1, const K2& key2, int number_of_arguments)
{
    ASSERT(number_of_arguments == this->push_counter);
    ASSERT(key1.event_id == key2.event_id);
    // Stack: [arguments]

    Push(key1.event_id);
    this->push_counter = 0;
    ++number_of_arguments;
    // Stack: [arguments], event_id

    int arguments_top = lua_gettop(L);
    if (arguments_top < number_of_arguments)
    {
        // 输出错误日志（关键：定位问题脚本）
        LOG_ERROR("eluna", "Eluna SetupStack ERROR: 参数数量不足！预期 %u 个参数，实际只有 %u 个", number_of_arguments, arguments_top);
        // 仅返回错误，不清理栈（避免栈错乱）
        return -1; // 返回负数标识"参数不足错误"，语义更清晰
    }
    int first_argument_index = arguments_top - number_of_arguments + 1;

    lua_insert(L, first_argument_index);
    // Stack: event_id, [arguments]

    bindings1->PushRefsFor(key1);
    if (bindings2)
        bindings2->PushRefsFor(key2);
    // Stack: event_id, [arguments], [functions]

    int number_of_functions = lua_gettop(L) - arguments_top;
    return number_of_functions;
}

/*
 * Replace one of the arguments pushed before `SetupStack` with a new value.
 */
template<typename T>
void Eluna::ReplaceArgument(T value, uint8 index)
{
    ASSERT(index < lua_gettop(L) && index > 0);
    // Stack: event_id, [arguments], [functions], [results]

    Eluna::Push(L, value);
    // Stack: event_id, [arguments], [functions], [results], value

    lua_replace(L, index + 1);
    // Stack: event_id, [arguments and value], [functions], [results]
}

/*
 * Call all event handlers registered to the event ID/entry combination and ignore any results.
 */
template<typename K1, typename K2>
void Eluna::CallAllFunctions(BindingMap<K1>* bindings1, BindingMap<K2>* bindings2, const K1& key1, const K2& key2)
{
    int number_of_arguments = this->push_counter;
    // Stack: [arguments]

    int number_of_functions = SetupStack(bindings1, bindings2, key1, key2, number_of_arguments);
    // Stack: event_id, [arguments], [functions] 或 保持原状态（如果参数不足）

    if (number_of_functions > 0)
    {
        while (number_of_functions > 0)
        {
            CallOneFunction(number_of_functions, number_of_arguments, 0);
            --number_of_functions;
            // Stack: event_id, [arguments], [functions - 1]
        }
        // Stack: event_id, [arguments]

        CleanUpStack(number_of_arguments);
        // Stack: (empty)
    }
    else if (number_of_functions == -1)
    {
        // 参数不足错误，清理栈并返回
        CleanUpStack(number_of_arguments);
        // Stack: (empty)
    }
}

/*
 * Call all event handlers registered to the event ID/entry combination,
 *   and returns `default_value` if ALL event handlers returned `default_value`,
 *   otherwise returns the opposite of `default_value`.
 */
template<typename K1, typename K2>
bool Eluna::CallAllFunctionsBool(BindingMap<K1>* bindings1, BindingMap<K2>* bindings2, const K1& key1, const K2& key2, bool default_value/* = false*/)
{
    bool result = default_value;
    // Note: number_of_arguments here does not count in eventID, which is pushed in SetupStack
    int number_of_arguments = this->push_counter;
    // Stack: [arguments]

    int number_of_functions = SetupStack(bindings1, bindings2, key1, key2, number_of_arguments);
    // Stack: event_id, [arguments], [functions] 或 保持原状态（如果参数不足）

    if (number_of_functions > 0)
    {
        while (number_of_functions > 0)
        {
            int r = CallOneFunction(number_of_functions, number_of_arguments, 1);
            --number_of_functions;
            // Stack: event_id, [arguments], [functions - 1], result

            if (lua_isboolean(L, r) && (lua_toboolean(L, r) == 1) != default_value)
                result = !default_value;

            lua_pop(L, 1);
            // Stack: event_id, [arguments], [functions - 1]
        }
        // Stack: event_id, [arguments]

        CleanUpStack(number_of_arguments);
        // Stack: (empty)
    }
    else if (number_of_functions == -1)
    {
        // 参数不足错误，清理栈并返回默认值
        CleanUpStack(number_of_arguments);
        // Stack: (empty)
    }
    return result;
}

#endif // _HOOK_HELPERS_H
