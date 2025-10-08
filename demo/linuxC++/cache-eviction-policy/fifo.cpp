/*
    FIFO (First-In, First-Out) Cache
    思路：最先进入缓存的项最先被淘汰。就是一个队列。
*/

#include <iostream>
#include <list>
#include <unordered_map>
#include <stdexcept> // for std::out_of_range

template <typename Key, typename Value>
class FIFOCache {
private:
    size_t capacity_member; // 缓存容量
    std::list<Key> queue_member; // 存储键的队列，表示进入顺序
    std::unordered_map<Key, Value> cache_map_member; // 存储键值对

public:
    FIFOCache(size_t capacity) : capacity_member(capacity) {
        if (capacity_member == 0) {
            throw std::invalid_argument("Capacity cannot be zero.");
        }
    }

    // 插入或更新一个项
    void put(const Key& key, const Value& value) {
        // 如果键已存在，更新值
        if (cache_map_member.count(key)) {
            cache_map_member[key] = value;
            return;
        }

        // 如果缓存已满，淘汰最老的项
        if (queue_member.size() >= capacity_member) {
            Key oldest_key = queue_member.front();
            queue_member.pop_front();
            cache_map_member.erase(oldest_key);
            // std::cout << "DEBUG: Evicting (FIFO): " << oldest_key << std::endl;
        }

        // 插入新项
        queue_member.push_back(key);
        cache_map_member[key] = value;
        // std::cout << "DEBUG: Adding (FIFO): " << key << std::endl;
    }

    // 获取一个项
    Value get(const Key& key) {
        if (cache_map_member.count(key)) {
            return cache_map_member[key];
        }
        throw std::out_of_range("Key not found in cache.");
    }

    // 辅助函数：获取当前缓存大小
    size_t size() const {
        return queue_member.size();
    }

    // 辅助函数：检查缓存是否包含某个键
    bool contains(const Key& key) const {
        return cache_map_member.count(key);
    }
};


// 示例用法
int main() {
    FIFOCache<int, std::string> fifo_cache(3);

    fifo_cache.put(1, "one");
    fifo_cache.put(2, "two");
    fifo_cache.put(3, "three");

    std::cout << "Cache size: " << fifo_cache.size() << std::endl; // Output: 3

    std::cout << "Get 1: " << fifo_cache.get(1) << std::endl; // Output: one (但不改变顺序)

    fifo_cache.put(4, "four"); // 1 "one" will be evicted

    std::cout << "Cache size: " << fifo_cache.size() << std::endl; // Output: 3
    std::cout << "Contains 1? " << fifo_cache.contains(1) << std::endl; // Output: 0 (false)
    std::cout << "Get 2: " << fifo_cache.get(2) << std::endl; // Output: two
    std::cout << "Get 4: " << fifo_cache.get(4) << std::endl; // Output: four

    try {
        fifo_cache.get(1);
    } catch (const std::out_of_range& e) {
        std::cout << "Error: " << e.what() << std::endl; // Output: Key not found in cache.
    }

    return 0;
}