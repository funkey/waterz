#include "StatisticsProvider.hpp"

template <typename RegionGraphType, typename Precision>
class MinAffinityProvider : public StatisticsProvider {

public:

	typedef Precision ValueType;
	typedef typename RegionGraphType::EdgeIdType EdgeIdType;

	MinAffinityProvider(RegionGraphType& regionGraph) :
		_minAffinities(regionGraph) {}

	void notifyNewEdge(EdgeIdType e) {

		_minAffinities[e] = std::numeric_limits<ValueType>::max();
	}

	void addAffinity(EdgeIdType e, ValueType affinity) {
	
		_minAffinities[e] = std::min(_minAffinities[e], affinity);
	}

	bool notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		if (_minAffinities[to] <= _minAffinities[from])
			// no change
			return false;

		_minAffinities[to] = _minAffinities[from];

		// score changed
		return true;
	}

	ValueType operator[](EdgeIdType e) const {

		return _minAffinities[e];
	}

private:

	typename RegionGraphType::template EdgeMap<ValueType> _minAffinities;
};
