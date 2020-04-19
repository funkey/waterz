class ScoringFunction:

    def __add__(self, other):
        return Add(self, other)

    def __radd__(self, other):
        return Add(other, self)

    def __sub__(self, other):
        return Subtract(self, other)

    def __rsub__(self, other):
        if other == 1.0:
            return OneMinus(self)
        return Subtract(other, self)

    def __mul__(self, other):
        return Multiply(self, other)

    def __rmul__(self, other):
        return Multiply(other, self)

    def __div__(self, other):
        return Divide(self, other)

    def __rdiv__(self, other):
        if other == 1.0:
            return Invert(self)
        return Divide(other, self)

    def __truediv__(self, other):
        return Divide(self, other)

    def __rtruediv__(self, other):
        if other == 1.0:
            return Invert(self)
        return Divide(other, self)

    def __pow__(self, other):
        if other == 2.0:
            return Square(self)
        raise RuntimeError("Powers other than 2 not implemented")

    def __repr__(self):
        raise RuntimeError("__repr__ not implemented")


class UnaryOperator(ScoringFunction):

    def __init__(self, a, operator):
        self.a = a
        self.operator = operator

    def __repr__(self):
        return f'{self.operator}<{self.a} >'


class OneMinus(UnaryOperator):

    def __init__(self, a):
        super(OneMinus, self).__init__(a, 'OneMinus')


class Invert(UnaryOperator):

    def __init__(self, a):
        super(Invert, self).__init__(a, 'Invert')


class Square(UnaryOperator):

    def __init__(self, a):
        super(Square, self).__init__(a, 'Square')


class BinaryOperator(ScoringFunction):

    def __init__(self, a, b, operator):
        self.a = a
        self.b = b
        self.operator = operator

    def __repr__(self):
        return f'{self.operator}<{self.a}, {self.b} >'


class Add(BinaryOperator):
    def __init__(self, a, b):
        super(Add, self).__init__(a, b, 'Add')


class Subtract(BinaryOperator):
    def __init__(self, a, b):
        super(Subtract, self).__init__(a, b, 'Subtract')


class Multiply(BinaryOperator):
    def __init__(self, a, b):
        super(Multiply, self).__init__(a, b, 'Multiply')


class Divide(BinaryOperator):
    def __init__(self, a, b):
        super(Divide, self).__init__(a, b, 'Divide')


class Step(BinaryOperator):
    def __init__(self, a, b):
        super(Step, self).__init__(a, b, 'Step')
