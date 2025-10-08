/*
    随机淘汰 (Random Replacement) 缓存
    思路：当缓存满时，随机选择一个项进行淘汰。

    实现：使用 std::unordered_map 存储数据，但淘汰时需要遍历找到一个随机的键。
    为了高效地随机选择，我们可以维护一个 std::vector<Key> 来存储所有当前缓存中的键，并随机选择其一。
*/

#include <iostream>
#include <unordered_map>
#include <vector>
#include <random>     // for std::default_random_engine, std::uniform_int_distribution
#include <algorithm>  // for std::find, std::remove
#include <stdexcept>

template <typename Key, typename Value>
class RandomCache {
private:
    size_t capacity_member;
    std::unordered_map<Key, Value> cache_map_member;
    std::vector<Key> keys_list_member; // 维护一个所有键的列表，用于随机选择

    // 随机数生成器
    std::default_random_engine rng_member;
    std::uniform_int_distribution<size_t> dist_member;

public:
    RandomCache(size_t capacity) : capacity_member(capacity) {
        if (capacity_member == 0) {
            throw std::invalid_argument("Capacity cannot be zero.");
        }
        // 初始化随机数生成器
        rng_member.seed(std::random_device()());
        dist_member = std::uniform_int_distribution<size_t>(0, capacity_member - 1);
    }

    void put(const Key& key, const Value& value) {
        if (cache_map_member.count(key)) {
            cache_map_member[key] = value; // 更新值
            return;
        }

        if (cache_map_member.size() >= capacity_member) {
            // 缓存已满，随机淘汰一个
            evict_random();
        }

        // 插入新项
        cache_map_member[key] = value;
        keys_list_member.push_back(key); // 将新键添加到列表
        // 每次 put 后重新调整分布，确保随机索引在当前 keys_list_member 范围内
        // 实际上，每次淘汰或插入都需要调整 dist_member
        // 更准确的做法是，每次生成随机数时，都根据 keys_list_member.size() 来生成
        dist_member = std::uniform_int_distribution<size_t>(0, keys_list_member.size() - 1);
        // std::cout << "DEBUG: Adding (Random): " << key << std::endl;
    }

    Value get(const Key& key) {
        if (cache_map_member.count(key)) {
            return cache_map_member[key];
        }
        throw std::out_of_range("Key not found in cache.");
    }

    void evict_random() {
        if (keys_list_member.empty()) return;

        // 获取当前 keys_list_member 的大小，并生成一个随机索引
        size_t index_to_evict = dist_member(rng_member) % keys_list_member.size();
        Key key_to_evict = keys_list_member[index_to_evict];

        // 从 keys_list_member 中删除被淘汰的键
        // 注意：std::vector 的删除操作是 O(N)，这使得随机淘汰在这里并非 O(1)。
        // 为了保持 O(1) 性能，需要使用更复杂的数据结构（如一个支持 O(1) 随机删除的哈希表 + 向量，或只用哈希表但随机选择键本身代价高）。
        // 这里只是为了演示随机淘汰的逻辑。
        keys_list_member.erase(keys_list_member.begin() + index_to_evict);
        cache_map_member.erase(key_to_evict);
        // std::cout << "DEBUG: Evicting (Random): " << key_to_evict << std::endl;

        // 如果 keys_list_member 不为空，更新随机数分布的上限
        if (!keys_list_member.empty()) {
             dist_member = std::uniform_int_distribution<size_t>(0, keys_list_member.size() - 1);
        } else {
             dist_member = std::uniform_int_distribution<size_t>(0, 0); // 或者重置为默认构造函数
        }
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
    RandomCache<int, std::string> random_cache(3);

    random_cache.put(1, "one");
    random_cache.put(2, "two");
    random_cache.put(3, "three");

    std::cout << "Cache size: " << random_cache.size() << std::endl; // Output: 3

    random_cache.put(4, "four"); // Evicts a random item (1, 2, or 3)
    std::cout << "Cache size: " << random_cache.size() << std::endl; // Output: 3

    random_cache.put(5, "five"); // Evicts another random item
    std::cout << "Cache size: " << random_cache.size() << std::endl; // Output: 3

    return 0;
}