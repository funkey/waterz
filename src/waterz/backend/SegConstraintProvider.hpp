#include "ConstraintProvider.hpp"
#include "DefaultDict.hpp"

#include <unordered_map>

template <typename RegionGraphType, typename SegType>
class SegConstraintProvider : public ConstraintProvider {

    typedef typename RegionGraphType::NodeIdType NodeIdType;

private:
    std::unordered_map<SegType, SegType> _constraint;

public:
	SegConstraintProvider(
            const SegType *segconstraint_data,
            const SegType *seg_data,
            size_t num_voxels) {

        // Collect all mapped constraint, accounting for when an object
        // is straddled across multiple constraint
        DefaultDict<SegType, DefaultDict<SegType, size_t>> constraint{
            DefaultDict<SegType, size_t>{0}
        };
		for (std::size_t i = 0; i < num_voxels; i++) {
            constraint[seg_data[i]][segconstraint_data[i]] += 1;
        }

        // Now we get a single dominant constraint per object
        for (const auto& pair : constraint.getContainer()) {
            const auto& segid = pair.first;
            const auto& mapped_constraint_ids = pair.second;
            _constraint[segid] = getDictMaxKey(mapped_constraint_ids.getContainer());
        }
    }

    inline bool notifyNodeMerge(NodeIdType from, NodeIdType to) {
        if (_constraint.at(to) == 0) {
            _constraint[to] = _constraint.at(from);
        }
        _constraint.erase(from);
        return true;
    }

    inline bool isConstrained(NodeIdType from, NodeIdType to, float score) const {
        if (_constraint.at(from) == 0 || _constraint.at(to) == 0)
            return false;
        if (_constraint.at(from) == _constraint.at(to))
            return false;
        return true;
    }
};
