#include "StatisticsProvider.hpp"

template <typename RegionGraphType, typename Precision>
class MaxAffinityProvider : public StatisticsProvider {

public:

	typedef Precision ValueType;
	typedef typename RegionGraphType::EdgeIdType EdgeIdType;

	MaxAffinityProvider(RegionGraphType& regionGraph) :
		_maxAffinities(regionGraph) {}

	void notifyNewEdge(EdgeIdType e) {

		_maxAffinities[e] = 0;
	}

	void addAffinity(EdgeIdType e, ValueType affinity) {
	
		_maxAffinities[e] = std::max(_maxAffinities[e], affinity);
	}

	void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		_maxAffinities[to] = std::max(_maxAffinities[to], _maxAffinities[from]);
	}


	ValueType operator[](EdgeIdType e) const {

		return _maxAffinities[e];
	}

private:

	typename RegionGraphType::template EdgeMap<ValueType> _maxAffinities;
};
