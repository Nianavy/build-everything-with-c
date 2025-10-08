/*
    思路：淘汰访问次数最少的项。这需要一个哈希表来存储键值对和频率，还需要一个机制来快速找到频率最低的项。
    这里，我会使用一个 std::map<int, std::list<Key>> (频率到键列表的映射) 和一个 std::unordered_map<Key, std::pair<Value, int>> 来存储值和频率。

    注意：LFU 的“历史包袱”问题和如何有效管理频率桶是很重要的。这里的实现会是最直接的 LFU，当你写完后，你会自然而然地思考它的缺点。
*/

#include <iostream>
#include <list>
#include <unordered_map>
#include <map> // Used for frequency map (sorted by frequency)
#include <stdexcept> // for std::out_of_range

template <typename Key, typename Value>
class LFUCache {
private:
    size_t capacity_member;
    int min_freq_member; // 当前缓存中最小的访问频率

    // 存储键值对和对应的频率 (Key -> {Value, Freq})
    std::unordered_map<Key, std::pair<Value, int>> cache_map_member;
    // 存储每个键在频率列表中的迭代器 (Key -> list<Key>::iterator)
    std::unordered_map<Key, typename std::list<Key>::iterator> key_iter_map_member;
    // 频率映射：频率 -> 拥有该频率的键的列表 (Freq -> list<Key>)
    std::map<int, std::list<Key>> freq_map_member;

    void evict() {
        // 从最小频率的列表中淘汰最老的项 (LFU + FIFO 结合)
        auto& min_freq_list = freq_map_member[min_freq_member];
        Key key_to_evict = min_freq_list.front();

        min_freq_list.pop_front();
        if (min_freq_list.empty()) {
            freq_map_member.erase(min_freq_member);
            // 注意：这里不需要更新 min_freq_member，因为下次 put 时会重新计算或更新
        }

        cache_map_member.erase(key_to_evict);
        key_iter_map_member.erase(key_to_evict);
        // std::cout << "DEBUG: Evicting (LFU): " << key_to_evict << std::endl;
    }

    void update_frequency(const Key& key, const Value& value, int old_freq) {
        // 从旧频率的列表中移除
        auto& old_list = freq_map_member[old_freq];
        old_list.erase(key_iter_map_member[key]);
        if (old_list.empty()) {
            freq_map_member.erase(old_freq);
            // 如果旧的最小频率列表为空，更新 min_freq_member
            if (old_freq == min_freq_member) {
                min_freq_member++; // 最小频率会增加
            }
        }

        // 增加频率
        int new_freq = old_freq + 1;
        cache_map_member[key] = {value, new_freq};
        freq_map_member[new_freq].push_back(key);
        key_iter_map_member[key] = --freq_map_member[new_freq].end(); // 存储新位置的迭代器
    }

public:
    LFUCache(size_t capacity) : capacity_member(capacity), min_freq_member(0) {
        if (capacity_member == 0) {
            throw std::invalid_argument("Capacity cannot be zero.");
        }
    }

    void put(const Key& key, const Value& value) {
        if (capacity_member == 0) return; // 容量为0直接返回

        if (cache_map_member.count(key)) {
            // 键已存在，更新值并更新频率
            update_frequency(key, value, cache_map_member[key].second);
        } else {
            // 键不存在
            if (cache_map_member.size() >= capacity_member) {
                evict(); // 缓存已满，执行淘汰
            }

            // 插入新项，频率为 1
            min_freq_member = 1; // 新插入的项总是最低频率1
            cache_map_member[key] = {value, 1};
            freq_map_member[1].push_back(key);
            key_iter_map_member[key] = --freq_map_member[1].end();
            // std::cout << "DEBUG: Adding (LFU): " << key << " freq 1" << std::endl;
        }
    }

    Value get(const Key& key) {
        if (!cache_map_member.count(key)) {
            throw std::out_of_range("Key not found in cache.");
        }

        // 获取值，并更新频率
        Value val = cache_map_member[key].first;
        update_frequency(key, val, cache_map_member[key].second);
        return val;
    }

    size_t size() const {
        return cache_map_member.size();
    }

    bool contains(const Key& key) const {
        return cache_map_member.count(key);
    }
};


// 示例用法
int main() {
    LFUCache<int, std::string> lfu_cache(3);

    lfu_cache.put(1, "one");   // freq: 1 -> [1]
    lfu_cache.put(2, "two");   // freq: 1 -> [1, 2]
    lfu_cache.put(3, "three"); // freq: 1 -> [1, 2, 3]

    std::cout << "Cache size: " << lfu_cache.size() << std::endl; // Output: 3

    lfu_cache.get(1); // freq: 1 -> [2, 3], 2 -> [1]
    lfu_cache.get(1); // freq: 1 -> [2, 3], 3 -> [1]
    lfu_cache.get(2); // freq: 1 -> [3], 2 -> [], 2 -> [2], 3 -> [1] (2 gets promoted)

    // Current:
    // min_freq_member = 1
    // freq_map_member:
    //   1: [3]
    //   2: [2]
    //   3: [1]

    lfu_cache.put(4, "four"); // Cache full. Evict '3' (min_freq_member = 1).
                              // freq: 1 -> [4], 2 -> [2], 3 -> [1]
                              // min_freq_member = 1

    std::cout << "Cache size: " << lfu_cache.size() << std::endl; // Output: 3
    std::cout << "Contains 3? " << lfu_cache.contains(3) << std::endl; // Output: 0 (false)
    std::cout << "Get 1: " << lfu_cache.get(1) << std::endl; // Output: one, freq 4
    std::cout << "Get 2: " << lfu_cache.get(2) << std::endl; // Output: two, freq 3
    std::cout << "Get 4: " << lfu_cache.get(4) << std::endl; // Output: four, freq 2

    return 0;
}