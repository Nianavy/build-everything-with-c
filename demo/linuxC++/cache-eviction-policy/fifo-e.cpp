/*
    思路：FIFO 的改进版，使用一个循环队列（或链表）和一个“使用位” (ref_bit_member)。
*/

#include <iostream>
#include <list>
#include <unordered_map>
#include <stdexcept> // for std::out_of_range

template <typename Key, typename Value>
struct ClockCacheEntry {
    Key key_member;
    Value value_member;
    bool ref_bit_member; // 引用位，表示最近是否被访问过

    ClockCacheEntry(const Key& k, const Value& v) : key_member(k), value_member(v), ref_bit_member(true) {}
};

template <typename Key, typename Value>
class ClockCache {
private:
    size_t capacity_member;
    // 存储缓存项的循环链表（std::list 在这里用作双向链表，但逻辑上是循环的）
    std::list<ClockCacheEntry<Key, Value>> clock_list_member;
    // 映射：键 -> 列表中对应项的迭代器，用于快速查找
    std::unordered_map<Key, typename std::list<ClockCacheEntry<Key, Value>>::iterator> key_map_member;
    // 指针，指向当前要检查的项（时钟指针）
    typename std::list<ClockCacheEntry<Key, Value>>::iterator clock_pointer_member;

public:
    ClockCache(size_t capacity) : capacity_member(capacity) {
        if (capacity_member == 0) {
            throw std::invalid_argument("Capacity cannot be zero.");
        }
        clock_pointer_member = clock_list_member.end(); // 初始化为end，表示列表为空
    }

    void put(const Key& key, const Value& value) {
        if (key_map_member.count(key)) {
            // 键已存在，更新值并设置引用位
            auto it = key_map_member[key];
            it->value_member = value;
            it->ref_bit_member = true; // 访问过，给它第二次机会
            return;
        }

        // 键不存在
        if (clock_list_member.size() >= capacity_member) {
            // 缓存已满，执行淘汰
            evict();
        }

        // 插入新项
        clock_list_member.push_back(ClockCacheEntry<Key, Value>(key, value));
        key_map_member[key] = --clock_list_member.end(); // 记录新项的迭代器

        // 如果之前列表为空，需要初始化时钟指针
        if (clock_list_member.size() == 1) {
            clock_pointer_member = clock_list_member.begin();
        }
        // std::cout << "DEBUG: Adding (CLOCK): " << key << std::endl;
    }

    Value get(const Key& key) {
        if (!key_map_member.count(key)) {
            throw std::out_of_range("Key not found in cache.");
        }

        auto it = key_map_member[key];
        it->ref_bit_member = true; // 访问过，给它第二次机会
        return it->value_member;
    }

    void evict() {
        if (clock_list_member.empty()) return; // 缓存为空，无需淘汰

        while (true) {
            if (clock_pointer_member == clock_list_member.end()) {
                clock_pointer_member = clock_list_member.begin(); // 循环到列表开头
            }

            if (clock_pointer_member->ref_bit_member) {
                // 有第二次机会，重置引用位，并移动指针
                clock_pointer_member->ref_bit_member = false;
                ++clock_pointer_member;
            } else {
                // 没有第二次机会，淘汰此项
                Key key_to_evict = clock_pointer_member->key_member;
                // std::cout << "DEBUG: Evicting (CLOCK): " << key_to_evict << std::endl;

                // 保存下一个元素的迭代器，因为当前元素将被删除
                auto next_pointer = std::next(clock_pointer_member);
                if (next_pointer == clock_list_member.end()) {
                    next_pointer = clock_list_member.begin(); // 循环回开头
                }

                key_map_member.erase(key_to_evict);
                clock_list_member.erase(clock_pointer_member);
                clock_pointer_member = next_pointer; // 更新时钟指针

                // 如果列表为空，重置指针
                if (clock_list_member.empty()) {
                    clock_pointer_member = clock_list_member.end();
                }
                break; // 淘汰完成
            }
        }
    }

    size_t size() const {
        return clock_list_member.size();
    }

    bool contains(const Key& key) const {
        return key_map_member.count(key);
    }
};


// 示例用法
int main() {
    ClockCache<int, std::string> clock_cache(3);

    clock_cache.put(1, "one");   // [1:T]
    clock_cache.put(2, "two");   // [1:T, 2:T]
    clock_cache.put(3, "three"); // [1:T, 2:T, 3:T] -> pointer starts at 1

    std::cout << "Cache size: " << clock_cache.size() << std::endl; // Output: 3

    std::cout << "Get 1: " << clock_cache.get(1) << std::endl; // 1:T (already T) -> [1:T, 2:T, 3:T]
    std::cout << "Get 2: " << clock_cache.get(2) << std::endl; // 2:T (already T) -> [1:T, 2:T, 3:T]

    clock_cache.put(4, "four"); // Cache full. Evict.
                                // Pointer at 1:T -> 1:F, ptr to 2
                                // Pointer at 2:T -> 2:F, ptr to 3
                                // Pointer at 3:T -> 3:F, ptr to 1 (wrap around)
                                // Pointer at 1:F -> Evict 1. Add 4.
                                // List: [2:F, 3:F, 4:T], ptr to 2

    std::cout << "Cache size: " << clock_cache.size() << std::endl; // Output: 3
    std::cout << "Contains 1? " << clock_cache.contains(1) << std::endl; // Output: 0 (false)
    std::cout << "Get 2: " << clock_cache.get(2) << std::endl; // Output: two, 2:T
    std::cout << "Get 4: " << clock_cache.get(4) << std::endl; // Output: four, 4:T

    clock_cache.put(5, "five"); // Cache full. Evict.
                                // Current: [2:T, 3:F, 4:T] ptr to 2
                                // Pointer at 2:T -> 2:F, ptr to 3
                                // Pointer at 3:F -> Evict 3. Add 5.
                                // List: [2:F, 4:T, 5:T], ptr to 4

    std::cout << "Cache size: " << clock_cache.size() << std::endl; // Output: 3
    std::cout << "Contains 3? " << clock_cache.contains(3) << std::endl; // Output: 0 (false)


    return 0;
}