#ifndef C_FRONTEND_TYPES_H
#define C_FRONTEND_TYPES_H

// everything a scoring function can refer to, and everything the frontend
// includes: the frontend itself is included once per variant, in a namespace

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "backend/IterativeRegionMerging.hpp"
#include "backend/MergeFunctions.hpp"
#include "backend/Operators.hpp"
#include "backend/types.hpp"
#include "backend/BinQueue.hpp"
#include "backend/PriorityQueue.hpp"
#include "backend/HistogramQuantileProvider.hpp"
#include "backend/VectorQuantileProvider.hpp"
#include "backend/basic_watershed.hpp"
#include "backend/region_graph.hpp"
#include "evaluate.hpp"

typedef uint64_t SegID;
typedef uint32_t GtID;
typedef float AffValue;
typedef float ScoreValue;
typedef RegionGraph<SegID> RegionGraphType;

struct Metrics {

	double voi_split;
	double voi_merge;
	double rand_split;
	double rand_merge;
};

struct Merge {

	SegID a;
	SegID b;
	SegID c;
	ScoreValue score;
};

struct ScoredEdge {

	ScoredEdge(SegID u_, SegID v_, ScoreValue score_) :
		u(u_),
		v(v_),
		score(score_) {}

	SegID u;
	SegID v;
	ScoreValue score;
};

struct WaterzState {

	int     context;
	Metrics metrics;
};

#endif
