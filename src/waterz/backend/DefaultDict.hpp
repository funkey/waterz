#ifndef WATERZ_DEFAULTDICT_H__
#define WATERZ_DEFAULTDICT_H__

#include <cstdint>
#include <unordered_map>

template<typename T>
uint64_t getDictSum(const T& map) {
    uint64_t sum = 0;
    for (const auto& pair : map) {
        sum += pair.second;
    }
    return sum;
}

template<typename T>
auto getDictMaxKey(const T& map) -> decltype(map.begin()->first) {
    if (map.empty()) {
        throw std::runtime_error("Map is empty");
    }
    auto max_key = map.begin()->first;
    auto max_value = map.begin()->second;
    for (const auto& pair : map) {
        if (pair.second > max_value) {
            max_key = pair.first;
            max_value = pair.second;
        }
    }
    return max_key;
}

template<typename K, typename V>
class DefaultDict {
private:
    std::unordered_map<K, V> container;
    V default_value;

public:
    DefaultDict() : default_value() {}
    DefaultDict(const V& default_val) : default_value(default_val) {}

    V& operator[](const K& key) {
        auto result = container.emplace(key, default_value);
        return result.first->second;
    }

    V operator[](const K& key) const {
        auto it = container.find(key);
        if (it != container.end()) {
            return it->second;
        }
        return default_value;
    }

    // V at(const K& key) const {
    //     auto it = container.find(key);
    //     if (it != container.end()) {
    //         return it->second;
    //     }
    //     return default_value;
    // }

    void erase(const K& key) {
        container.erase(key);
    }

    const std::unordered_map<K, V>& getContainer() const { return container; }
};


#endif // WATERZ_DEFAULTDICT_H__
