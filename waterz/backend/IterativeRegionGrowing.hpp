#ifndef WATERZ_ITERATIVE_REGION_GROWING_H__
#define WATERZ_ITERATIVE_REGION_GROWING_H__

#include "BinQueue.hpp"

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
		_fragments(seeds),
		_direction(0) {

		initialize();
	}

	void grow() {

		while (!_boundaryQueue.empty()) {

			BoundaryVoxel next = _boundaryQueue.top();
			_boundaryQueue.pop();

			// only if still unassigned
			if (_fragments[next.z][next.y][next.x] != 0)
				continue;

			// assign voxel to region
			_fragments[next.z][next.y][next.x] = next.region;

			addBoundaryVoxels(next.z, next.y, next.x);
		}
	}

private:

	struct BoundaryVoxel {

		size_t z, y, x;
		NodeIdType region;
	};

	typedef BinQueue<BoundaryVoxel, 256> QueueType;

	enum Direction {

		PosZ = 0,
		NegZ,
		PosY,
		NegY,
		PosX,
		NegX,

		NumDirections
	};

	void initialize() {

		_depth  = _fragments.shape()[0];
		_height = _fragments.shape()[1];
		_width  = _fragments.shape()[2];

		for (size_t z = 0; z < _depth; z++)
			for (size_t y = 0; y < _height; y++)
				for (size_t x = 0; x < _width; x++)
					addBoundaryVoxels(z, y, x);
	}

	void addBoundaryVoxels(size_t z, size_t y, size_t x) {

		NodeIdType id = _fragments[z][y][x];

		if (id == 0)
			return;

		_direction++;

		for (int i = 0; i < NumDirections; i++) {

			int d = (i+_direction)%NumDirections;

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

			NodeIdType neighborId = _fragments[nz][ny][nx];

			// only if neighbor is unassigned
			if (neighborId != 0)
				continue;

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

			BoundaryVoxel boundaryVoxel= {nz, ny, nx, id};
			_boundaryQueue.push(boundaryVoxel, (int)((1.0-affinity)*255));
		}
	}

	const affinity_graph_ref<ScoreType>& _affinityGraph;
	volume_ref<NodeIdType>&              _fragments;

	size_t _depth;
	size_t _height;
	size_t _width;

	QueueType _boundaryQueue;

	int _direction;
};

#endif // WATERZ_ITERATIVE_REGION_GROWING_H__

