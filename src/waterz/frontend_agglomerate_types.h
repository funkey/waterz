#ifndef C_FRONTEND_TYPES_H
#define C_FRONTEND_TYPES_H

// everything a scoring function can refer to

#include <vector>

#include "backend/IterativeRegionMerging.hpp"
#include "backend/MergeFunctions.hpp"
#include "backend/Operators.hpp"
#include "backend/types.hpp"
#include "backend/BinQueue.hpp"
#include "backend/PriorityQueue.hpp"
#include "backend/HistogramQuantileProvider.hpp"
#include "backend/VectorQuantileProvider.hpp"
#include "evaluate.hpp"

typedef uint64_t SegID;
typedef uint32_t GtID;
typedef float AffValue;
typedef float ScoreValue;
typedef RegionGraph<SegID> RegionGraphType;

#endif
