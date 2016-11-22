#ifndef WATERZ_OPERATORS_H__
#define WATERZ_OPERATORS_H__

#include <functional>

template <typename ScoreFunction1, typename ScoreFunction2, template <typename> class Op>
class BinaryOperator : public ScoreFunction1, public ScoreFunction2 {

public:

	typedef typename ScoreFunction1::ScoreType  ScoreType;
	typedef typename ScoreFunction1::NodeIdType NodeIdType;
	typedef typename ScoreFunction1::EdgeIdType EdgeIdType;

	template <typename AffinityMapType, typename SizeMapType>
	BinaryOperator(AffinityMapType& affinities, SizeMapType& regionSizes) :
		ScoreFunction1(affinities, regionSizes),
		ScoreFunction2(affinities, regionSizes) {}

	inline ScoreType operator()(EdgeIdType e) {

		return _op(ScoreFunction1::operator()(e), ScoreFunction2::operator()(e));
	}

	void notifyNodeMerge(NodeIdType from, NodeIdType to) {

		ScoreFunction1::notifyNodeMerge(from, to);
		ScoreFunction2::notifyNodeMerge(from, to);
	}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {
	
		ScoreFunction1::notifyEdgeMerge(from, to);
		ScoreFunction2::notifyEdgeMerge(from, to);
	}

private:

	Op<ScoreType> _op;
};

template <typename ScoreFunction, template <typename> class Op>
class UnaryOperator : public ScoreFunction {

public:

	typedef typename ScoreFunction::ScoreType  ScoreType;
	typedef typename ScoreFunction::NodeIdType NodeIdType;
	typedef typename ScoreFunction::EdgeIdType EdgeIdType;

	template <typename AffinityMapType, typename SizeMapType>
	UnaryOperator(AffinityMapType& affinities, SizeMapType& regionSizes) :
		ScoreFunction(affinities, regionSizes) {}

	inline ScoreType operator()(EdgeIdType e) {

		return _op(ScoreFunction::operator()(e));
	}

	inline void notifyNodeMerge(NodeIdType from, NodeIdType to) {

		ScoreFunction::notifyNodeMerge(from, to);
	}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {
	
		ScoreFunction::notifyEdgeMerge(from, to);
	}

private:

	Op<ScoreType> _op;
};

template <typename T>
struct one_minus {
	T operator()(const T& x) const { return 1.0 - x; }
};
template <typename T>
using OneMinus = UnaryOperator<T, one_minus>;

template <typename T>
struct invert {
	T operator()(const T& x) const { return 1.0/x; }
};
template <typename T>
using Invert = UnaryOperator<T, invert>;

template <typename T>
struct square {
	T operator()(const T& x) const { return x*x; }
};
template <typename T>
using Square = UnaryOperator<T, square>;

template <typename T1, typename T2>
using Add = BinaryOperator<T1, T2, std::plus>;

template <typename T1, typename T2>
using Multiply = BinaryOperator<T1, T2, std::multiplies>;

template <typename T1, typename T2>
using Divide = BinaryOperator<T1, T2, std::divides>;

#endif // WATERZ_OPERATORS_H__

