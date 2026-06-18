#ifndef WATERZ_SEMANTIC_TAINT_CONSTRAINT_PROVIDER_H__
#define WATERZ_SEMANTIC_TAINT_CONSTRAINT_PROVIDER_H__

#include "ConstraintProvider.hpp"
#include "DefaultDict.hpp"

#include <unordered_set>
#include <vector>

template <typename RegionGraphType, typename SemValue, typename SegType>
class SemanticTaintConstraintProvider : public ConstraintProvider {

    typedef typename RegionGraphType::NodeIdType NodeIdType;

private:
    DefaultDict<SegType, size_t> _taint_counts{0};
    DefaultDict<SegType, size_t> _total_counts{0};
    float _threshold;

public:
    SemanticTaintConstraintProvider(
        const SemValue* semantic_data,
        const SegType* seg_data,
        size_t num_voxels,
        const std::vector<SemValue>& taint_labels,
        float threshold
    ) : _threshold(threshold) {
        std::unordered_set<SemValue> taint_set(taint_labels.begin(), taint_labels.end());
        for (size_t i = 0; i < num_voxels; i++) {
            _total_counts[seg_data[i]] += 1;
            if (taint_set.count(semantic_data[i])) {
                _taint_counts[seg_data[i]] += 1;
            }
        }
    }

    inline bool notifyNodeMerge(NodeIdType from, NodeIdType to) override {
        _taint_counts[to] += _taint_counts[from];
        _taint_counts.erase(from);
        _total_counts[to] += _total_counts[from];
        _total_counts.erase(from);
        return true;
    }

    inline bool isConstrained(NodeIdType from, NodeIdType to, float score) const override {
        size_t total_from = _total_counts[from];
        size_t total_to = _total_counts[to];
        bool from_tainted = total_from > 0 && (float)_taint_counts[from] / total_from > _threshold;
        bool to_tainted = total_to > 0 && (float)_taint_counts[to] / total_to > _threshold;
        // tainted segments can only merge with other tainted segments
        if (from_tainted != to_tainted)
            return true;
        return false;
    }
};

#endif // WATERZ_SEMANTIC_TAINT_CONSTRAINT_PROVIDER_H__
