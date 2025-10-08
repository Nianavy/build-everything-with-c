/*
    思路：将缓存分为多个 LRU 段。通常是两个：一个“新生代”或“受保护段” (Protected Segment) 和一个“老生代”或“主要段” (Main Segment)。
    新加入的项通常进入新生代，如果频繁访问则晋升到老生代。淘汰时先从新生代的 LRU 尾部淘汰。

    实现：本质上是两个独立的 LRU 缓存的组合，但它们之间有晋升/降级/淘汰的规则。
*/

#include <iostream>
#include <list>
#include <unordered_map>
#include <stdexcept>

template <typename Key, typename Value>
class SLRUCache {
private:
    size_t capacity_member;
    size_t protected_capacity_member; // 受保护段的容量
    
    // Protected Segment: 存储最近频繁访问的项 (也是 LRU 结构)
    std::list<std::pair<Key, Value>> protected_list_member;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> protected_map_member;

    // Main Segment: 存储新进入或不那么频繁访问的项 (也是 LRU 结构)
    std::list<std::pair<Key, Value>> main_list_member;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> main_map_member;

    // 辅助函数：将项移动到指定列表的头部
    void move_to_front(std::list<std::pair<Key, Value>>& lst,
                       typename std::list<std::pair<Key, Value>>::iterator it) {
        lst.splice(lst.begin(), lst, it);
    }

    // 辅助函数：从列表中删除最旧的项
    void evict_from_list(std::list<std::pair<Key, Value>>& lst,
                         std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator>& map) {
        if (lst.empty()) return;
        Key key_to_evict = lst.back().first;
        lst.pop_back();
        map.erase(key_to_evict);
        // std::cout << "DEBUG: Evicting from list: " << key_to_evict << std::endl;
    }

public:
    // capacity: 总容量
    // protected_ratio: 受保护段占总容量的比例 (0.0 - 1.0)
    SLRUCache(size_t capacity, double protected_ratio = 0.5) : capacity_member(capacity) {
        if (capacity_member == 0) {
            throw std::invalid_argument("Capacity cannot be zero.");
        }
        if (protected_ratio < 0.0 || protected_ratio > 1.0) {
            throw std::invalid_argument("Protected ratio must be between 0 and 1.");
        }
        protected_capacity_member = static_cast<size_t>(capacity_member * protected_ratio);
        if (protected_capacity_member == 0 && capacity_member > 0 && protected_ratio > 0) {
            protected_capacity_member = 1; // 至少保证一个容量
        }
        if (protected_capacity_member >= capacity_member) { // 保护段不能大于等于总容量
             protected_capacity_member = capacity_member > 1 ? capacity_member - 1 : 0;
        }
         // 确保主段至少有一个容量，如果总容量大于1
        if (capacity_member > 0 && protected_capacity_member == capacity_member) {
             if (capacity_member > 1) { // 至少留一个给主段
                 protected_capacity_member--;
             } else { // 如果容量是1，保护段只能是0，主段是1
                 protected_capacity_member = 0;
             }
        }
    }

    void put(const Key& key, const Value& value) {
        // 1. 检查 Protected Segment
        if (protected_map_member.count(key)) {
            auto it = protected_map_member[key];
            it->second = value;
            move_to_front(protected_list_member, it);
            return;
        }

        // 2. 检查 Main Segment
        if (main_map_member.count(key)) {
            // 从 Main Segment 移除，晋升到 Protected Segment
            auto it = main_map_member[key];
            Value old_value = it->second; // 保存旧值，因为 put 可能会传入新值
            main_list_member.erase(it);
            main_map_member.erase(key);

            // 如果 Protected Segment 已满，需要淘汰一个到 Main Segment
            if (protected_list_member.size() >= protected_capacity_member) {
                // 将 Protected Segment 最旧的项降级到 Main Segment
                Key key_to_demote = protected_list_member.back().first;
                Value value_to_demote = protected_list_member.back().second;
                evict_from_list(protected_list_member, protected_map_member);

                // 将降级的项添加到 Main Segment 头部 (这是 SLRU 的一个变种行为，也可添加到尾部)
                main_list_member.push_front({key_to_demote, value_to_demote});
                main_map_member[key_to_demote] = main_list_member.begin();
                 // std::cout << "DEBUG: Demoting (SLRU): " << key_to_demote << " from protected to main" << std::endl;
            }
            // 将当前项添加到 Protected Segment 头部
            protected_list_member.push_front({key, value});
            protected_map_member[key] = protected_list_member.begin();
            // std::cout << "DEBUG: Promoting (SLRU): " << key << " from main to protected" << std::endl;
            return;
        }

        // 3. 键不存在于任何段中，作为新项加入 Main Segment
        // 如果总容量已满，需要淘汰
        if ((protected_list_member.size() + main_list_member.size()) >= capacity_member) {
            // 总是从 Main Segment 的尾部淘汰
            evict_from_list(main_list_member, main_map_member);
             // std::cout << "DEBUG: Evicting (SLRU): from main segment" << std::endl;
        }

        // 加入新项到 Main Segment 头部
        main_list_member.push_front({key, value});
        main_map_member[key] = main_list_member.begin();
        // std::cout << "DEBUG: Adding (SLRU): " << key << " to main" << std::endl;
    }

    Value get(const Key& key) {
        // 1. 检查 Protected Segment
        if (protected_map_member.count(key)) {
            auto it = protected_map_member[key];
            move_to_front(protected_list_member, it);
            return it->second;
        }

        // 2. 检查 Main Segment
        if (main_map_member.count(key)) {
            // 从 Main Segment 移除，晋升到 Protected Segment
            auto it = main_map_member[key];
            Value val = it->second;
            main_list_member.erase(it);
            main_map_member.erase(key);

            // 如果 Protected Segment 已满，需要淘汰一个到 Main Segment
            if (protected_list_member.size() >= protected_capacity_member) {
                // 将 Protected Segment 最旧的项降级到 Main Segment
                Key key_to_demote = protected_list_member.back().first;
                Value value_to_demote = protected_list_member.back().second;
                evict_from_list(protected_list_member, protected_map_member);

                // 将降级的项添加到 Main Segment 头部
                main_list_member.push_front({key_to_demote, value_to_demote});
                main_map_member[key_to_demote] = main_list_member.begin();
                // std::cout << "DEBUG: Demoting (SLRU): " << key_to_demote << " from protected to main" << std::endl;
            }
            // 将当前项添加到 Protected Segment 头部
            protected_list_member.push_front({key, val});
            protected_map_member[key] = protected_list_member.begin();
            // std::cout << "DEBUG: Promoting (SLRU): " << key << " from main to protected" << std::endl;
            return val;
        }

        throw std::out_of_range("Key not found in cache.");
    }

    size_t size() const {
        return protected_list_member.size() + main_list_member.size();
    }

    bool contains(const Key& key) const {
        return protected_map_member.count(key) || main_map_member.count(key);
    }
};


// 示例用法
int main() {
    SLRUCache<int, std::string> slru_cache(5, 0.4); // 总容量5，受保护段容量 5 * 0.4 = 2

    // Main: [], Protected: []
    slru_cache.put(1, "one");   // Main: [1], Protected: []
    slru_cache.put(2, "two");   // Main: [2, 1], Protected: []
    slru_cache.put(3, "three"); // Main: [3, 2, 1], Protected: []
    slru_cache.put(4, "four");  // Main: [4, 3, 2, 1], Protected: []
    slru_cache.put(5, "five");  // Main: [5, 4, 3, 2, 1], Protected: [] - Total 5

    std::cout << "Cache size: " << slru_cache.size() << std::endl; // Output: 5

    std::cout << "Get 1: " << slru_cache.get(1) << std::endl; // 1从Main晋升到Protected
    // Main: [5, 4, 3, 2], Protected: [1]
    std::cout << "Get 2: " << slru_cache.get(2) << std::endl; // 2从Main晋升到Protected
    // Main: [5, 4, 3], Protected: [2, 1]

    std::cout << "Get 3: " << slru_cache.get(3) << std::endl; // 3从Main晋升到Protected
    // Protected Segment已满 (容量2)。
    // Protected 最旧的1被降级到 Main。
    // Main: [3, 5, 4], Protected: [3, 2] (3晋升，1降级)
    // 注意：1被降级到 Main 的头部，而不是尾部，这是一种常见的 SLRU 变体实现，
    // 以防止“刚被降级又被淘汰”的问题。

    std::cout << "Cache size: " << slru_cache.size() << std::endl; // Output: 5

    slru_cache.put(6, "six"); // Main Segment 已满 (3)，总容量也满。
                             // 从 Main Segment 淘汰最旧的 4。6加入 Main。
    // Main: [6, 3, 5], Protected: [3, 2]

    std::cout << "Contains 1? " << slru_cache.contains(1) << std::endl; // Output: 0 (false if 1 got evicted later from main)
                                                                         // (Actually, 1 got demoted, it's still in main)
    std::cout << "Contains 4? " << slru_cache.contains(4) << std::endl; // Output: 0 (false)

    return 0;
}