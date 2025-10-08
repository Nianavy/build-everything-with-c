/*
    思路：淘汰最长时间未被访问的项。每次访问一个项，都将其移动到“最近使用”的位置（通常是链表头部）。当缓存满时，淘汰链表尾部的项。
*/

#include <iostream>
#include <list>
#include <unordered_map>
#include <stdexcept> // for std::out_of_range

template <typename Key, typename Value>
class LRUCache {
private:
    size_t capacity_member; // 缓存容量

    // 存储缓存项的双向链表，Key-Value 对作为链表节点
    // 链表头部是最近使用的，尾部是最久未使用的
    std::list<std::pair<Key, Value>> lru_list_member;

    // 存储键到链表迭代器的映射，用于 O(1) 查找和 O(1) 更新链表位置
    // Key -> std::list<std::pair<Key, Value>>::iterator
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> key_map_member;

    // 辅助函数：将一个项移动到链表头部
    void move_to_front(typename std::list<std::pair<Key, Value>>::iterator it) {
        lru_list_member.splice(lru_list_member.begin(), lru_list_member, it);
    }

    // 辅助函数：删除链表尾部的项（最久未使用的）
    void evict_lru() {
        if (lru_list_member.empty()) return;

        Key key_to_evict = lru_list_member.back().first;
        // std::cout << "DEBUG: Evicting (LRU): " << key_to_evict << std::endl;
        lru_list_member.pop_back();
        key_map_member.erase(key_to_evict);
    }

public:
    LRUCache(size_t capacity) : capacity_member(capacity) {
        if (capacity_member == 0) {
            throw std::invalid_argument("Capacity cannot be zero.");
        }
    }

    // 插入或更新一个项
    void put(const Key& key, const Value& value) {
        if (key_map_member.count(key)) {
            // 键已存在：更新值，并将该项移动到链表头部
            auto it = key_map_member[key];
            it->second = value; // 更新值
            move_to_front(it);  // 移动到头部
            // std::cout << "DEBUG: Updating (LRU) and moving to front: " << key << std::endl;
        } else {
            // 键不存在：
            // 1. 如果缓存已满，淘汰最久未使用的项
            if (lru_list_member.size() >= capacity_member) {
                evict_lru();
            }
            // 2. 插入新项到链表头部
            lru_list_member.push_front({key, value});
            // 3. 在哈希表中记录新项的迭代器
            key_map_member[key] = lru_list_member.begin();
            // std::cout << "DEBUG: Adding (LRU) to front: " << key << std::endl;
        }
    }

    // 获取一个项
    Value get(const Key& key) {
        if (!key_map_member.count(key)) {
            throw std::out_of_range("Key not found in cache.");
        }

        // 键存在：获取值，并将该项移动到链表头部
        auto it = key_map_member[key];
        Value val = it->second;
        move_to_front(it); // 移动到头部
        // std::cout << "DEBUG: Getting (LRU) and moving to front: " << key << std::endl;
        return val;
    }

    // 辅助函数：获取当前缓存大小
    size_t size() const {
        return lru_list_member.size();
    }

    // 辅助函数：检查缓存是否包含某个键
    bool contains(const Key& key) const {
        return key_map_member.count(key);
    }
};


// 示例用法
int main() {
    LRUCache<int, std::string> lru_cache(3);

    lru_cache.put(1, "one");   // List: [1]
    lru_cache.put(2, "two");   // List: [2, 1]
    lru_cache.put(3, "three"); // List: [3, 2, 1]

    std::cout << "Cache size: " << lru_cache.size() << std::endl; // Output: 3

    std::cout << "Get 1: " << lru_cache.get(1) << std::endl; // List: [1, 3, 2] (1 moved to front)
    std::cout << "Get 2: " << lru_cache.get(2) << std::endl; // List: [2, 1, 3] (2 moved to front)

    lru_cache.put(4, "four"); // Cache full. Evict 3. List: [4, 2, 1] (4 added to front)

    std::cout << "Cache size: " << lru_cache.size() << std::endl; // Output: 3
    std::cout << "Contains 3? " << lru_cache.contains(3) << std::endl; // Output: 0 (false)
    std::cout << "Get 1: " << lru_cache.get(1) << std::endl; // Output: one, List: [1, 4, 2]
    std::cout << "Get 4: " << lru_cache.get(4) << std::endl; // Output: four, List: [4, 1, 2]

    try {
        lru_cache.get(3);
    } catch (const std::out_of_range& e) {
        std::cout << "Error: " << e.what() << std::endl; // Output: Key not found in cache.
    }

    return 0;
}