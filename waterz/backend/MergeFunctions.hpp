#ifndef MERGE_FUNCTIONS_H__
#define MERGE_FUNCTIONS_H__

#include <algorithm>
#include <random>
#include "Operators.hpp"
#include "VectorQuantileProvider.hpp"
#include "MaxKValues.hpp"

/**
 * Scores edges with min size of incident regions.
 */
template <typename SizeMapType>
class MinSize {

public:

	typedef typename SizeMapType::RegionGraphType RegionGraphType;
	typedef float                                 ScoreType;
	typedef typename RegionGraphType::NodeIdType  NodeIdType;
	typedef typename RegionGraphType::EdgeIdType  EdgeIdType;

	template <typename AffinityMapType>
	MinSize(AffinityMapType& affinities, SizeMapType& regionSizes) :
		_regionGraph(regionSizes.getRegionGraph()),
		_regionSizes(regionSizes) {}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	inline ScoreType operator()(EdgeIdType e) {

		return std::min(
				_regionSizes[_regionGraph.edge(e).u],
				_regionSizes[_regionGraph.edge(e).v]);
	}

	inline void notifyNodeMerge(NodeIdType from, NodeIdType to) {

		_regionSizes[to] += _regionSizes[from];
	}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {}

private:

	const RegionGraphType& _regionGraph;
	SizeMapType            _regionSizes;
};

/**
 * Scores edges with max size of incident regions.
 */
template <typename SizeMapType>
class MaxSize {

public:

	typedef typename SizeMapType::RegionGraphType RegionGraphType;
	typedef float                                 ScoreType;
	typedef typename RegionGraphType::NodeIdType  NodeIdType;
	typedef typename RegionGraphType::EdgeIdType  EdgeIdType;

	template <typename AffinityMapType>
	MaxSize(AffinityMapType& affinities, SizeMapType& regionSizes) :
		_regionGraph(regionSizes.getRegionGraph()),
		_regionSizes(regionSizes) {}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	inline ScoreType operator()(EdgeIdType e) {

		return std::max(
				_regionSizes[_regionGraph.edge(e).u],
				_regionSizes[_regionGraph.edge(e).v]);
	}

	inline void notifyNodeMerge(NodeIdType from, NodeIdType to) {

		_regionSizes[to] += _regionSizes[from];
	}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {}

private:

	const RegionGraphType& _regionGraph;
	SizeMapType            _regionSizes;
};

/**
 * Scores edges with min affinity.
 */
template <typename AffinityMapType>
class MinAffinity {

public:

	typedef typename AffinityMapType::RegionGraphType RegionGraphType;
	typedef typename AffinityMapType::ValueType       ScoreType;
	typedef typename RegionGraphType::NodeIdType      NodeIdType;
	typedef typename RegionGraphType::EdgeIdType      EdgeIdType;

	template <typename SizeMapType>
	MinAffinity(AffinityMapType& affinities, SizeMapType& regionSizes) :
		_affinities(affinities) {}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	inline ScoreType operator()(EdgeIdType e) {

		return _affinities[e];
	}

	void notifyNodeMerge(NodeIdType from, NodeIdType to) {}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		_affinities[to] = std::min(_affinities[to], _affinities[from]);
	}

private:

	AffinityMapType& _affinities;
};

/**
 * Scores edges with a quantile affinity. The quantile is approximated by 
 * keeping track of a histogram of affinity values. This approximation is exact 
 * if the number of bins corresponds to the discretization of the affinities. 
 * The default is to use 256 bins.
 *
 * Affinities are assumed to be in [0,1].
 */
template <typename AffinityMapType, int Quantile, template <int Q, typename P> class QuantileProvider = VectorQuantileProvider>
class QuantileAffinity {

public:

	typedef typename AffinityMapType::RegionGraphType RegionGraphType;
	typedef typename AffinityMapType::ValueType       ScoreType;
	typedef typename RegionGraphType::NodeIdType      NodeIdType;
	typedef typename RegionGraphType::EdgeIdType      EdgeIdType;

	template <typename SizeMapType>
	QuantileAffinity(AffinityMapType& affinities, SizeMapType& regionSizes) :
		_quantileProviders(affinities.getRegionGraph()) {

		std::cout << "Initializing affinity quantile providers..." << std::endl;
		for (EdgeIdType e = 0; e < affinities.getRegionGraph().numEdges(); e++)
			_quantileProviders[e].add(affinities[e]);
	}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	ScoreType operator()(EdgeIdType e) {

		return _quantileProviders[e].value();
	}

	void notifyNodeMerge(NodeIdType from, NodeIdType to) {}

	void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		_quantileProviders[to].merge(_quantileProviders[from]);
		_quantileProviders[from].clear();
	}

private:

	// a quantile provider for each edge
	typename RegionGraphType::template EdgeMap<QuantileProvider<Quantile, ScoreType>> _quantileProviders;
};

/**
 * Scores edges with max affinity.
 */
template <typename AffinityMapType>
class MaxAffinity {

public:

	typedef typename AffinityMapType::RegionGraphType RegionGraphType;
	typedef typename AffinityMapType::ValueType       ScoreType;
	typedef typename RegionGraphType::NodeIdType      NodeIdType;
	typedef typename RegionGraphType::EdgeIdType      EdgeIdType;

	template <typename SizeMapType>
	MaxAffinity(AffinityMapType& affinities, SizeMapType& regionSizes) :
		_affinities(affinities) {}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	inline ScoreType operator()(EdgeIdType e) {

		return _affinities[e];
	}

	void notifyNodeMerge(NodeIdType from, NodeIdType to) {}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		_affinities[to] = std::max(_affinities[to], _affinities[from]);
	}

private:

	AffinityMapType& _affinities;
};

/**
 * Scores edges with the mean of the max k affinities.
 */
template <typename AffinityMapType, int K>
class MaxKAffinity {

public:

	typedef typename AffinityMapType::RegionGraphType RegionGraphType;
	typedef typename AffinityMapType::ValueType       ScoreType;
	typedef typename RegionGraphType::NodeIdType      NodeIdType;
	typedef typename RegionGraphType::EdgeIdType      EdgeIdType;

	template <typename SizeMapType>
	MaxKAffinity(AffinityMapType& affinities, SizeMapType& regionSizes) :
		_maxKValues(affinities.getRegionGraph()) {

		std::cout << "Initializing max k vectors..." << std::endl;
		for (EdgeIdType e = 0; e < affinities.getRegionGraph().numEdges(); e++)
			_maxKValues[e].push(affinities[e]);
	}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	inline ScoreType operator()(EdgeIdType e) {

		return _maxKValues[e].average();
	}

	void notifyNodeMerge(NodeIdType from, NodeIdType to) {}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		_maxKValues[to].merge(_maxKValues[from]);
	}

private:

	// a quantile provider for each edge
	typename RegionGraphType::template EdgeMap<MaxKValues<ScoreType,K>> _maxKValues;
};

/**
 * Scores edges with a random number between 0 and 1.
 */
template <typename RegionGraphType>
class Random {

public:

	typedef float                                ScoreType;
	typedef typename RegionGraphType::NodeIdType NodeIdType;
	typedef typename RegionGraphType::EdgeIdType EdgeIdType;

	template <typename AffinityMapType, typename SizeMapType>
	Random(AffinityMapType&, SizeMapType&) {}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	inline ScoreType operator()(EdgeIdType e) {

		return ScoreType(rand())/RAND_MAX;
	}

	inline void notifyNodeMerge(NodeIdType from, NodeIdType to) {}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {}
};

/**
 * Scores edges with a constant.
 */
template <typename RegionGraphType, int C>
class Const {

public:

	typedef float                                ScoreType;
	typedef typename RegionGraphType::NodeIdType NodeIdType;
	typedef typename RegionGraphType::EdgeIdType EdgeIdType;

	template <typename AffinityMapType, typename SizeMapType>
	Const(AffinityMapType&, SizeMapType&) {}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	inline ScoreType operator()(EdgeIdType e) {

		return C;
	}

	inline void notifyNodeMerge(NodeIdType from, NodeIdType to) {}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {}
};

#endif // MERGE_FUNCTIONS_H__

