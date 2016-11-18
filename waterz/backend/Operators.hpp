#include <functional>

template <typename ScoreFunction1, typename ScoreFunction2, typename Op>
class BinaryOperator {

public:

	typedef typename ScoreFunction1::ScoreType  ScoreType;
	typedef typename ScoreFunction1::NodeIdType NodeIdType;
	typedef typename ScoreFunction1::EdgeIdType EdgeIdType;

	BinaryOperator(ScoreFunction1 score1, ScoreFunction2 score2) :
		_score1(score1),
		_score2(score2) {}

	inline ScoreType operator()(EdgeIdType e) {

		return _op(_score1(e), _score2(e));
	}

	inline void notifyNodeMerge(NodeIdType a, NodeIdType b, NodeIdType target) {

		_score1.notifyNodeMerge(a, b, target);
		_score2.notifyNodeMerge(a, b, target);
	}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {
	
		_score1.notifyEdgeMerge(from, to);
		_score2.notifyEdgeMerge(from, to);
	}

private:

	ScoreFunction1 _score1;
	ScoreFunction2 _score2;

	Op _op;
};

template <typename ScoreFunction, typename Op>
class UnaryOperator {

public:

	typedef typename ScoreFunction::ScoreType  ScoreType;
	typedef typename ScoreFunction::NodeIdType NodeIdType;
	typedef typename ScoreFunction::EdgeIdType EdgeIdType;

	UnaryOperator(ScoreFunction score) :
		_score(score) {}

	inline ScoreType operator()(EdgeIdType e) {

		return _op(_score(e));
	}

	inline void notifyNodeMerge(NodeIdType a, NodeIdType b, NodeIdType target) {

		_score.notifyNodeMerge(a, b, target);
	}

	inline void notifyEdgeMerge(EdgeIdType from, EdgeIdType to) {
	
		_score.notifyEdgeMerge(from, to);
	}

private:

	ScoreFunction _score;

	Op _op;
};

struct OneMinus {
	template <typename T>
	inline T operator()(T t) { return 1.0 - t; }
};

struct Invert {
	template <typename T>
	inline T operator()(T t) { return 1.0/t; }
};

struct Square {
	template <typename T>
	inline T operator()(T t) { return t*t; }
};

struct Add {
	template <typename T1, typename T2>
	inline T1 operator()(T1 a, T2 b) { return a + b; }
};

struct Multiply {
	template <typename T1, typename T2>
	inline T1 operator()(T1 a, T2 b) { return a*b; }
};

struct Divide {
	template <typename T1, typename T2>
	inline T1 operator()(T1 a, T2 b) { return a/b; }
};

/**
 * Convenience wrappers for common operators.
 */

template <typename ScoreFunction>
UnaryOperator<ScoreFunction, OneMinus>
oneMinus(ScoreFunction a) {
	return UnaryOperator<ScoreFunction, OneMinus>(a);
}

template <typename ScoreFunction>
UnaryOperator<ScoreFunction, Invert>
invert(ScoreFunction a) {
	return UnaryOperator<ScoreFunction, Invert>(a);
}

template <typename ScoreFunction>
UnaryOperator<ScoreFunction, Square>
square(ScoreFunction a) {
	return UnaryOperator<ScoreFunction, Square>(a);
}

template <typename ScoreFunction1, typename ScoreFunction2>
BinaryOperator<ScoreFunction1, ScoreFunction2, Add>
add(ScoreFunction1 a, ScoreFunction2 b) {

	return BinaryOperator<ScoreFunction1, ScoreFunction2, Add>(a, b);
}

template <typename ScoreFunction1, typename ScoreFunction2>
BinaryOperator<ScoreFunction1, ScoreFunction2, Multiply>
multiply(ScoreFunction1 a, ScoreFunction2 b) {

	return BinaryOperator<ScoreFunction1, ScoreFunction2, Multiply>(a, b);
}

template <typename ScoreFunction1, typename ScoreFunction2>
BinaryOperator<ScoreFunction1, ScoreFunction2, Divide>
divide(ScoreFunction1 a, ScoreFunction2 b) {

	return BinaryOperator<ScoreFunction1, ScoreFunction2, Divide>(a, b);
}
