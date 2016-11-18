#ifndef MERGE_FUNCTIONS_H__
#define MERGE_FUNCTIONS_H__

#include "Operators.hpp"

/**
 * Scores edges with min size of incident regions.
 */
template <typename SizeMapType>
class MinSize {

public:

	typedef typename SizeMapType::RegionGraphType RegionGraphType;
	typedef typename SizeMapType::ValueType       ScoreType;
	typedef typename RegionGraphType::NodeIdType  NodeIdType;
	typedef typename RegionGraphType::EdgeIdType  EdgeIdType;

	MinSize(SizeMapType& regionSizes) :
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

	inline void notifyNodeMerge(NodeIdType a, NodeIdType b, NodeIdType target) {

		_regionSizes[target] = _regionSizes[a] + _regionSizes[b];
	}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {}

private:

	const RegionGraphType& _regionGraph;
	SizeMapType&           _regionSizes;
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

	MinAffinity(AffinityMapType& affinities) :
		_affinities(affinities) {}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	inline ScoreType operator()(EdgeIdType e) {

		return _affinities[e];
	}

	void notifyNodeMerge(NodeIdType a, NodeIdType b, NodeIdType target) {}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		_affinities[to] = std::min(_affinities[to], _affinities[from]);
	}

private:

	AffinityMapType& _affinities;
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

	MaxAffinity(AffinityMapType& affinities) :
		_affinities(affinities) {}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	inline ScoreType operator()(EdgeIdType e) {

		return _affinities[e];
	}

	void notifyNodeMerge(NodeIdType a, NodeIdType b, NodeIdType target) {}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		_affinities[to] = std::max(_affinities[to], _affinities[from]);
	}

private:

	AffinityMapType& _affinities;
};

/**
 * Scores edges with median affinity.
 */
template <typename AffinityMapType>
class MedianAffinity {

public:

	typedef typename AffinityMapType::RegionGraphType RegionGraphType;
	typedef typename AffinityMapType::ValueType       ScoreType;
	typedef typename RegionGraphType::NodeIdType      NodeIdType;
	typedef typename RegionGraphType::EdgeIdType      EdgeIdType;

	MedianAffinity(AffinityMapType& affinities) :
		_affinities(affinities),
		_affiliatedEdges(affinities.getRegionGraph()) {}

	/**
	 * Get the score for an edge. An edge will be merged the earlier, the 
	 * smaller its score is.
	 */
	ScoreType operator()(EdgeIdType e) {

		std::vector<EdgeIdType>& affiliatedEdges = _affiliatedEdges[e];

		// initial edges have their own affinity
		if (affiliatedEdges.size() == 0)
			return _affinities[e];

		// edges resulting from merges consult their affiliated edges

		auto median = affiliatedEdges.begin() + affiliatedEdges.size()/2;
		std::nth_element(
				affiliatedEdges.begin(),
				median,
				affiliatedEdges.end(),
				[this](EdgeIdType a, EdgeIdType b){

					return _affinities[a] < _affinities[b];
				}
		);

		return _affinities[*median];
	}

	void notifyNodeMerge(NodeIdType a, NodeIdType b, NodeIdType target) {}

	void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		if (_affiliatedEdges[from].size() == 0)
			// 'from' is an initial edge
			_affiliatedEdges[to].push_back(from);
		else
			// 'from' is a compound edge, copy its affiliated edges to the new 
			// affiliated edge list
			std::copy(
					_affiliatedEdges[from].begin(),
					_affiliatedEdges[from].end(),
					std::back_inserter(_affiliatedEdges[to]));

		// clear affiliated edges of merged region edges -- they are not 
		// needed anymore
		_affiliatedEdges[from].clear();
	}

private:

	const AffinityMapType& _affinities;

	// for every new edge between regions u and v, the edges of the initial RAG 
	// between any child of u and any child of v
	//
	// initial edges will have this empty
	typename RegionGraphType::template EdgeMap<std::vector<EdgeIdType>> _affiliatedEdges;
};

#endif // MERGE_FUNCTIONS_H__

