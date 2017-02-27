#ifndef WATERZ_ITERATIVE_REGION_GROWING_H__
#define WATERZ_ITERATIVE_REGION_GROWING_H__

template <typename NodeIdType, typename ScoreType>
class IterativeRegionGrowing {

public:

	/**
	 * Assumes that seeds are consecutively numbered, starting at 1. Background 
	 * is 0.
	 */
	IterativeRegionGrowing(
			const affinity_graph_ref<ScoreType>& affinityGraph,
			volume_ref<NodeIdType>&              seeds) :
		_affinityGraph(affinityGraph),
		_fragments(seeds) {

		initialize(seeds);
	}

	void grow() {

		while (!queuesEmpty()) {

			for (size_t i = 0; i < _boundaryQueues.size(); i++) {

				if (_boundaryQueues[i].empty())
					continue;

				//std::cout << "growing region " << i+1 << std::endl;

				BoundaryVoxel next = _boundaryQueues[i].top();
				_boundaryQueues[i].pop();

				//std::cout << "considering (" << next.z << "," << next.y << "," << next.x << ")" << std::endl;

				// only if still unassigned
				if (_fragments[next.z][next.y][next.x] != 0)
					continue;

				//std::cout << "adding (" << next.z << "," << next.y << "," << next.x << ")" << std::endl;

				// assign to seed with index i: id = i + 1
				_fragments[next.z][next.y][next.x] = i + 1;

				//std::cout << "assigned to label " << i+1 << std::endl;

				addBoundaryVoxels(next.z, next.y, next.x);
			}
		}
	}

private:

	struct BoundaryVoxel {

		size_t z, y, x;
		ScoreType score;

		bool operator<(const BoundaryVoxel& other) const {

			return score < other.score;
		}
	};

	typedef std::priority_queue<BoundaryVoxel> QueueType;

	enum Direction {

		PosZ = 0,
		NegZ,
		PosY,
		NegY,
		PosX,
		NegX,

		NumDirections
	};

	void initialize(volume_ref<NodeIdType>& seeds) {

		_depth  = seeds.shape()[0];
		_height = seeds.shape()[1];
		_width  = seeds.shape()[2];

		std::cout << "size of seeds volume: " << _depth << ", " << _height << ", " << _width << std::endl;
		std::cout << "size of affinity graph: "
				<< _affinityGraph.shape()[0] << ", "
				<< _affinityGraph.shape()[1] << ", "
				<< _affinityGraph.shape()[2] << ", "
				<< _affinityGraph.shape()[3] << std::endl;

		size_t numSeeds = 0;
		for (size_t z = 0; z < _depth; z++)
			for (size_t y = 0; y < _height; y++)
				for (size_t x = 0; x < _width; x++)
					numSeeds = std::max(seeds[z][y][x], numSeeds);

		std::cout << "found " << numSeeds << " seeds" << std::endl;

		_boundaryQueues.resize(numSeeds);

		for (size_t z = 0; z < _depth; z++)
			for (size_t y = 0; y < _height; y++)
				for (size_t x = 0; x < _width; x++)
					addBoundaryVoxels(z, y, x);
	}

	bool queuesEmpty() {

		for (auto q : _boundaryQueues)
			if (!q.empty())
				return false;
		return true;
	}

	void addBoundaryVoxels(size_t z, size_t y, size_t x) {

		NodeIdType id = _fragments[z][y][x];

		if (id == 0)
			return;

		for (int d = 0; d < NumDirections; d++) {

			Direction dir = (Direction)d;

			size_t nz = z;
			size_t ny = y;
			size_t nx = x;

			if (dir == PosZ)
				nz++;
			if (dir == NegZ)
				nz--;
			if (dir == PosY)
				ny++;
			if (dir == NegY)
				ny--;
			if (dir == PosX)
				nx++;
			if (dir == NegX)
				nx--;

			// only if neighbor exists
			if (nz >= _depth || ny >= _height || nx >= _width)
				continue;

			NodeIdType id = _fragments[nz][ny][nx];

			// only if neighbor is unassigned
			if (id != 0)
				continue;

			//std::cout << "adding boundary voxel" << std::endl;
			//std::cout << "(z,y,x) == (" << z << "," << y << "," << x << std::endl;
			//std::cout << "(nz,ny,nx) == (" << nz << "," << ny << "," << nx << std::endl;
			//std::cout << "d == " << d << std::endl;

			//std::cout << "affinity edge at "
					//<< (d/2) << " "
					//<< (z-(dir==NegZ)) << " "
					//<< (y-(dir==NegY)) << " "
					//<< (x-(dir==NegX)) << std::endl;

			/**
			 * How to get from (x,y,z) and offset to correct affinity 
			 * value (example only on x):
			 *
			 *  0   1   2   3
			 *
			 *  x      = 2
			 *  offset = -1
			 *  nx     = 1
			 *  => affinity at 1 = x + offset
			 *
			 *  x      = 0
			 *  offset = 1
			 *  nx     = 1
			 *  => affinity at 0 = x
			 *
			 *  ==> x + min(offset,0)
			 */
			ScoreType affinity = _affinityGraph[d/2]
					[z-(dir==NegZ)]
					[y-(dir==NegY)]
					[x-(dir==NegX)];

			//std::cout << "affinity = " << affinity << std::endl;
			//std::cout << "new boundary voxel at (" << nz << "," << ny << "," << nx << ")" << std::endl;

			BoundaryVoxel boundaryVoxel= {nz, ny, nx, affinity};
			_boundaryQueues[id].push(boundaryVoxel);
		}
	}

	const affinity_graph_ref<ScoreType>& _affinityGraph;
	volume_ref<NodeIdType>&              _fragments;

	size_t _depth;
	size_t _height;
	size_t _width;

	std::vector<QueueType> _boundaryQueues;
};

#endif // WATERZ_ITERATIVE_REGION_GROWING_H__

