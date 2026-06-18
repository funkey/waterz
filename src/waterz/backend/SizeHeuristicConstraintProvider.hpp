#include "StatisticsProvider.hpp"
#include "DefaultDict.hpp"

template <typename RegionGraphType, typename SegType>
class SizeHeuristicConstraintProvider : public ConstraintProvider {

    typedef typename RegionGraphType::NodeIdType NodeIdType;

private:
    DefaultDict<SegType, size_t> _size{0};
    float size_heuristic_aff_threshold;
    size_t size_heuristic_small_threshold;
    size_t size_heuristic_large_threshold;

public:
	SizeHeuristicConstraintProvider(
            const std::vector<size_t>& precomputed_sizes,
            float size_heuristic_aff_threshold,
            size_t size_heuristic_small_threshold,
            size_t size_heuristic_large_threshold
    ):
    size_heuristic_aff_threshold(size_heuristic_aff_threshold),
    size_heuristic_small_threshold(size_heuristic_small_threshold),
    size_heuristic_large_threshold(size_heuristic_large_threshold)
    {
		for (std::size_t i = 0; i < precomputed_sizes.size(); i++) {
            if (precomputed_sizes[i] > 0)
                _size[static_cast<SegType>(i)] = precomputed_sizes[i];
        }
    }

    inline bool notifyNodeMerge(NodeIdType from, NodeIdType to) override {
        _size[to] += _size[from];
        _size.erase(from);
        return true;  // statistics changed
    }

    inline bool isConstrained(NodeIdType from, NodeIdType to, float score) const override {
        if (score < size_heuristic_aff_threshold)
            return false;
        if (_size[from] < size_heuristic_small_threshold)
            return false;
        if (_size[to] < size_heuristic_small_threshold)
            return false;
        if ((_size[from] + _size[to]) < size_heuristic_large_threshold)
            return false;
        return true;
    }
};
