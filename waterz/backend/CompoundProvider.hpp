#ifndef WATERZ_COMPOUND_PROVIDER_H__
#define WATERZ_COMPOUND_PROVIDER_H__

/**
 * Combines statistics providers into a single provider, which inherits from all 
 * the other ones.
 */

// inherits from all given types
template <typename Head, typename ... Tail>
class CompoundProvider : public Head, public CompoundProvider<Tail...> {

public:

	typedef Head HeadType;
	typedef CompoundProvider<Tail...> Parent;

	template <typename RegionGraphType>
	CompoundProvider(RegionGraphType& regionGraph) :
		Head(regionGraph),
		Parent(regionGraph) {}

	template <typename EdgeIdType>
	void notifyNewEdge(EdgeIdType e) {
	
		Head::notifyNewEdge(e);
		Parent::notifyNewEdge(e);
	}

	template <typename EdgeIdType, typename ScoreType>
	void addAffinity(EdgeIdType e, ScoreType affinity) {

		Head::addAffinity(e, affinity);
		Parent::addAffinity(e, affinity);
	}

	template <typename NodeIdType>
	void addVoxel(NodeIdType n, std::size_t x, std::size_t y, std::size_t z) {

		Head::addVoxel(n, x, y, z);
		Parent::addVoxel(n, x, y, z);
	}

	template<typename NodeIdType>
	void notifyNodeMerge(NodeIdType from, NodeIdType to) {

		Head::notifyNodeMerge(from, to);
		Parent::notifyNodeMerge(from, to);
	}

	template<typename EdgeIdType>
	void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {

		Head::notifyEdgeMerge(from, to);
		Parent::notifyEdgeMerge(from, to);
	}
};

// end of recursion
template <typename T>
class CompoundProvider<T> : public T {
public:
	typedef T HeadType;
};

#endif // WATERZ_COMPOUND_PROVIDER_H__

