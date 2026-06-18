#ifndef REGION_GRAPH_H__
#define REGION_GRAPH_H__

#include <algorithm>
#include <vector>
#include <unordered_map>
#include <limits>
#include <cassert>

template <typename ID>
struct RegionGraphEdge {

	typedef ID NodeIdType;

	NodeIdType u;
	NodeIdType v;

	RegionGraphEdge() : u(0), v(0) {}
	RegionGraphEdge(NodeIdType u_, NodeIdType v_) : u(u_), v(v_) {}
};

// forward declaration
template <typename ID>
class RegionGraph;

template<typename ID>
class RegionGraphNodeMapBase {

public:

	typedef RegionGraph<ID> RegionGraphType;

	RegionGraphType& getRegionGraph() { return _regionGraph; }

	const RegionGraphType& getRegionGraph() const { return _regionGraph; }

protected:

	RegionGraphNodeMapBase(RegionGraphType& regionGraph) :
		_regionGraph(regionGraph) {

		_regionGraph.registerNodeMap(this);
	}

	virtual ~RegionGraphNodeMapBase() {

		_regionGraph.deregisterNodeMap(this);
	}

private:

	friend RegionGraphType;

	virtual void onNewNode(ID id) = 0;

	RegionGraphType& _regionGraph;
};

template<typename ID, typename T, typename Container>
class RegionGraphNodeMap : public RegionGraphNodeMapBase<ID> {

public:

	typedef T ValueType;

	typedef RegionGraph<ID> RegionGraphType;

	RegionGraphNodeMap(RegionGraphType& regionGraph) :
		RegionGraphNodeMapBase<ID>(regionGraph),
		_values(regionGraph.numNodes()) {}

	RegionGraphNodeMap(RegionGraphType& regionGraph, Container&& values) :
		RegionGraphNodeMapBase<ID>(regionGraph),
		_values(std::move(values)) {}

	inline typename Container::const_reference operator[](ID i) const { return _values[i]; }
	inline typename Container::reference operator[](ID i) { return _values[i]; }

private:

	void onNewNode(ID id) {

		_values.push_back(T());
	}

	Container _values;
};

template<typename ID>
class RegionGraphEdgeMapBase {

public:

	typedef RegionGraph<ID> RegionGraphType;

	RegionGraphType& getRegionGraph() { return _regionGraph; }

	const RegionGraphType& getRegionGraph() const { return _regionGraph; }

protected:

	RegionGraphEdgeMapBase(RegionGraphType& regionGraph) :
		_regionGraph(regionGraph) {

		_regionGraph.registerEdgeMap(this);
	}

	virtual ~RegionGraphEdgeMapBase() {

		_regionGraph.deregisterEdgeMap(this);
	}

private:

	friend RegionGraphType;

	virtual void onNewEdge(std::size_t id) = 0;

	RegionGraphType& _regionGraph;
};

template<typename ID, typename T, typename Container>
class RegionGraphEdgeMap : public RegionGraphEdgeMapBase<ID> {

public:

	typedef T ValueType;

	typedef RegionGraph<ID> RegionGraphType;

	RegionGraphEdgeMap(RegionGraphType& regionGraph) :
		RegionGraphEdgeMapBase<ID>(regionGraph),
		_values(regionGraph.edges().size()) {}

	inline typename Container::const_reference operator[](std::size_t i) const { return _values[i]; }
	inline typename Container::reference operator[](std::size_t i) { return _values[i]; }

private:

	void onNewEdge(std::size_t id) {

		_values.push_back(T());
	}

	Container _values;
};

template <typename ID>
class RegionGraph {

public:

	typedef ID                          NodeIdType;
	typedef std::size_t                 EdgeIdType;

	typedef RegionGraphEdge<NodeIdType> EdgeType;

	template <typename T, typename Container = std::vector<T>>
	using NodeMap = RegionGraphNodeMap<ID, T, Container>;

	template <typename T, typename Container = std::vector<T>>
	using EdgeMap = RegionGraphEdgeMap<ID, T, Container>;

	static const EdgeIdType NoEdge = std::numeric_limits<EdgeIdType>::max();

	RegionGraph(ID numNodes = 0) :
		_numNodes(numNodes),
		_incEdges(numNodes),
		_adjMap(numNodes) {}

	ID numNodes() const { return _numNodes; }

	std::size_t numEdges() const { return _edges.size(); }

	ID addNode() {

		NodeIdType id = _numNodes;
		_numNodes++;
		_incEdges.emplace_back();
		_adjMap.emplace_back();

		for (RegionGraphNodeMapBase<ID>* map : _nodeMaps)
			map->onNewNode(id);

		return id;
	}

	EdgeIdType addEdge(NodeIdType u, NodeIdType v) {

		EdgeIdType id = _edges.size();
		_edges.push_back(EdgeType(std::min(u, v), std::max(u, v)));

		_incEdges[u].push_back(id);
		_incEdges[v].push_back(id);
		_adjMap[u][v] = id;
		_adjMap[v][u] = id;

		for (RegionGraphEdgeMapBase<ID>* map : _edgeMaps)
			map->onNewEdge(id);

		return id;
	}

	void removeEdge(EdgeIdType e) {

		NodeIdType u = _edges[e].u;
		NodeIdType v = _edges[e].v;
		removeIncEdge(u, e);
		removeIncEdge(v, e);
		_adjMap[u].erase(v);
		_adjMap[v].erase(u);
	}

	void moveEdge(EdgeIdType e, NodeIdType u, NodeIdType v) {

		// three possibilities:
		//
		//   1. nothing changed (unlikely, callers responsibility)
		//   2. only u or v changed
		//      order independent, four subcases
		//   3. u and v changed

		NodeIdType pu = _edges[e].u;
		NodeIdType pv = _edges[e].v;

		// is pu already one of the new nodes?
		if (pu == u) {

			// keep pu, update pv -> v
			moveEdgeNodeV(e, v);

		} else if (pu == v) {

			// keep pu, update pv -> u
			moveEdgeNodeV(e, u);

		} else {

			// is pv already one of the new nodes?
			if (pv == v) {

				// keep pv, update pu -> u
				moveEdgeNodeU(e, u);

			} else if (pv == u) {

				// keep pv, update pu -> u
				moveEdgeNodeU(e, v);

			} else {

				// none of them is equal to the new nodes
				moveEdgeNodeU(e, u);
				moveEdgeNodeV(e, v);
			}
		}

		// ensure new ids are sorted
		if (_edges[e].u > _edges[e].v)
			std::swap(_edges[e].u, _edges[e].v);

		assert(std::min(u, v) == _edges[e].u);
		assert(std::max(u, v) == _edges[e].v);
		assert(findEdge(u, v) == e);
		assert(std::find(incEdges(u).begin(), incEdges(u).end(), e) != incEdges(u).end());
		assert(std::find(incEdges(v).begin(), incEdges(v).end(), e) != incEdges(v).end());
	}

	inline const EdgeType& edge(EdgeIdType e) const { return _edges[e]; }

	inline const std::vector<EdgeType>& edges() const { return _edges; }

	inline const std::vector<EdgeIdType>& incEdges(ID node) const { return _incEdges[node]; }

	inline std::vector<EdgeIdType> takeIncEdges(ID node) {
		std::vector<EdgeIdType> result;
		result.swap(_incEdges[node]);
		return result;
	}

	inline NodeIdType getOpposite(NodeIdType n, EdgeIdType e) const {

		return (_edges[e].u == n ? _edges[e].v : _edges[e].u);
	}

	/**
	 * Fast edge reassignment: move edge e from oldNode to newNode.
	 * Caller must ensure one endpoint of e is oldNode.
	 * Does NOT touch oldNode's incEdges (caller handles that).
	 */
	void reassignEdge(EdgeIdType e, NodeIdType oldNode, NodeIdType newNode) {

		NodeIdType other = getOpposite(oldNode, e);

		// Update adjacency maps
		_adjMap[oldNode].erase(other);
		_adjMap[other].erase(oldNode);
		_adjMap[newNode][other] = e;
		_adjMap[other][newNode] = e;

		// Update edge endpoints
		if (_edges[e].u == oldNode)
			_edges[e].u = newNode;
		else
			_edges[e].v = oldNode == _edges[e].v ? newNode : _edges[e].v;

		// Ensure u < v
		if (_edges[e].u > _edges[e].v)
			std::swap(_edges[e].u, _edges[e].v);

		// Add to newNode's incident list (don't remove from oldNode - caller owns that)
		_incEdges[newNode].push_back(e);
	}

	/**
	 * Remove edge from graph but don't touch incEdges of the given skipNode.
	 * Used when the caller already owns/cleared that node's incident list.
	 */
	void removeEdgeSkipNode(EdgeIdType e, NodeIdType skipNode) {

		NodeIdType u = _edges[e].u;
		NodeIdType v = _edges[e].v;
		NodeIdType other = (u == skipNode) ? v : u;
		removeIncEdge(other, e);
		_adjMap[u].erase(v);
		_adjMap[v].erase(u);
	}

	/**
	 * Replace oldEdge (between survivor and neighbor) with newEdge
	 * (being reassigned from oldNode to survivor). Fuses removeEdge +
	 * reassignEdge to avoid redundant incEdge and adjMap operations.
	 * Caller must have already taken oldNode's incEdges.
	 */
	void replaceEdge(EdgeIdType oldEdge, EdgeIdType newEdge,
			NodeIdType survivor, NodeIdType neighbor, NodeIdType oldNode) {

		// Remove oldEdge from survivor's and neighbor's incEdges
		removeIncEdge(survivor, oldEdge);
		removeIncEdge(neighbor, oldEdge);

		// Remove newEdge from neighbor's incEdges (oldNode's already taken)
		removeIncEdge(neighbor, newEdge);

		// Update newEdge endpoints: oldNode -> survivor
		if (_edges[newEdge].u == oldNode)
			_edges[newEdge].u = survivor;
		else
			_edges[newEdge].v = survivor;
		if (_edges[newEdge].u > _edges[newEdge].v)
			std::swap(_edges[newEdge].u, _edges[newEdge].v);

		// Add newEdge to survivor's and neighbor's incEdges
		_incEdges[survivor].push_back(newEdge);
		_incEdges[neighbor].push_back(newEdge);

		// Update adjMaps: survivor↔neighbor now points to newEdge
		_adjMap[survivor][neighbor] = newEdge;
		_adjMap[neighbor][survivor] = newEdge;

		// Clean up oldNode's adjMap entries
		_adjMap[oldNode].erase(neighbor);
		_adjMap[neighbor].erase(oldNode);
	}

	/**
	 * Find the edge connecting u and v. Returns NoEdge, if there is none.
	 */
	inline EdgeIdType findEdge(NodeIdType u, NodeIdType v) {

		auto it = _adjMap[u].find(v);
		if (it != _adjMap[u].end())
			return it->second;
		return NoEdge;
	}

private:

	friend RegionGraphNodeMapBase<ID>;
	friend RegionGraphEdgeMapBase<ID>;

	void registerNodeMap(RegionGraphNodeMapBase<ID>* nodeMap) {

		_nodeMaps.push_back(nodeMap);
	}

	void deregisterNodeMap(RegionGraphNodeMapBase<ID>* nodeMap) {

		auto it = std::find(_nodeMaps.begin(), _nodeMaps.end(), nodeMap);
		if (it != _nodeMaps.end())
			_nodeMaps.erase(it);
	}

	void registerEdgeMap(RegionGraphEdgeMapBase<ID>* edgeMap) {

		_edgeMaps.push_back(edgeMap);
	}

	void deregisterEdgeMap(RegionGraphEdgeMapBase<ID>* edgeMap) {

		auto it = std::find(_edgeMaps.begin(), _edgeMaps.end(), edgeMap);
		if (it != _edgeMaps.end())
			_edgeMaps.erase(it);
	}

	inline void moveEdgeNodeV(EdgeIdType e, NodeIdType v) {

		NodeIdType oldV = _edges[e].v;
		NodeIdType otherNode = _edges[e].u;
		removeIncEdge(oldV, e);
		_adjMap[oldV].erase(otherNode);
		_adjMap[otherNode].erase(oldV);
		_incEdges[v].push_back(e);
		_adjMap[v][otherNode] = e;
		_adjMap[otherNode][v] = e;
		_edges[e].v = v;
	}

	inline void moveEdgeNodeU(EdgeIdType e, NodeIdType u) {

		NodeIdType oldU = _edges[e].u;
		NodeIdType otherNode = _edges[e].v;
		removeIncEdge(oldU, e);
		_adjMap[oldU].erase(otherNode);
		_adjMap[otherNode].erase(oldU);
		_incEdges[u].push_back(e);
		_adjMap[u][otherNode] = e;
		_adjMap[otherNode][u] = e;
		_edges[e].u = u;
	}

	inline void removeIncEdge(NodeIdType n, EdgeIdType e) {

		auto it = std::find(_incEdges[n].begin(), _incEdges[n].end(), e);
		assert(it != _incEdges[n].end());
		std::swap(*it, _incEdges[n].back());
		_incEdges[n].pop_back();
		assert(std::find(_incEdges[n].begin(), _incEdges[n].end(), e) == _incEdges[n].end());
	}

	ID _numNodes;

	std::vector<EdgeType> _edges;

	std::vector<std::vector<EdgeIdType>> _incEdges;

	// per-node adjacency map: neighbor -> edge id (O(1) findEdge)
	std::vector<std::unordered_map<NodeIdType, EdgeIdType>> _adjMap;

	std::vector<RegionGraphNodeMapBase<ID>*> _nodeMaps;
	std::vector<RegionGraphEdgeMapBase<ID>*> _edgeMaps;
};

#endif // REGION_GRAPH_H__

