#include "ConstraintProvider.hpp"
#include "DefaultDict.hpp"

#include <cstdint>
#include <unordered_map>

template <typename RegionGraphType, typename SemValue, typename SegType>
class SemanticConstraintProvider : public ConstraintProvider {

	typedef SemValue ValueType;
	typedef typename RegionGraphType::EdgeIdType EdgeIdType;
    typedef typename RegionGraphType::NodeIdType NodeIdType;

private:
    DefaultDict<SegType, DefaultDict<SemValue, size_t>> _semantic{
        DefaultDict<SemValue, size_t>{0}
    };
    float semantic_aff_threshold;
    uint64_t semantic_size_threshold;
    float semantic_signal_ratio;

public:
	SemanticConstraintProvider(
        const SemValue *semantic_data,
        const SegType *seg_data,
        size_t num_voxels,
        float semantic_aff_threshold,
        uint64_t semantic_size_threshold,
        float semantic_signal_ratio
    ) :
    semantic_aff_threshold(semantic_aff_threshold),
    semantic_size_threshold(semantic_size_threshold),
    semantic_signal_ratio(semantic_signal_ratio)
    {
		for (std::size_t i = 0; i < num_voxels; i++) {
            _semantic[seg_data[i]][semantic_data[i]] += 1;
        }
    }

    inline bool notifyNodeMerge(NodeIdType from, NodeIdType to) {
        for (auto k : _semantic[from].getContainer()) {
            // _semantic[to][k] += _semantic[from][k];
            _semantic[to][k.first] += k.second;
        }
        _semantic.erase(from);
        return true;
    }

    inline bool isConstrained(NodeIdType from, NodeIdType to, float score) const {

        if (score < semantic_aff_threshold)
            return false;

        auto max_sem1_label = getDictMaxKey(_semantic[from].getContainer());
        auto max_sem1 = _semantic[from][max_sem1_label];
        auto total_sem1 = getDictSum(_semantic[from].getContainer());
        auto max_sem2_label = getDictMaxKey(_semantic[to].getContainer());
        auto max_sem2 = _semantic[to][max_sem2_label];
        auto total_sem2 = getDictSum(_semantic[to].getContainer());

        if (total_sem1 < semantic_size_threshold)
            return false;
        if (total_sem2 < semantic_size_threshold)
            return false;
        if (max_sem1 < semantic_signal_ratio * total_sem1)
            return false;
        if (max_sem2 < semantic_signal_ratio * total_sem2)
            return false;
        if (max_sem1_label == 0 || max_sem2_label == 0)
            return false;
        if (max_sem1_label == max_sem2_label)
            return false;
        return true;
    }
};
