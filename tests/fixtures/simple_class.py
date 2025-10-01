"""Simple Python class for testing tree-sitter parsing."""


class Calculator:
    """A simple calculator class."""

    def __init__(self):
        """Initialize the calculator."""
        self.result = 0

    def add(self, x, y):
        """Add two numbers."""
        return x + y

    def subtract(self, x, y):
        """Subtract y from x."""
        return x - y

    def multiply(self, x, y):
        """Multiply two numbers."""
        return x * y

    def divide(self, x, y):
        """Divide x by y."""
        if y == 0:
            raise ValueError("Cannot divide by zero")
        return x / y


class ScientificCalculator(Calculator):
    """Extended calculator with scientific functions."""

    def power(self, base, exponent):
        """Calculate base raised to exponent."""
        return base ** exponent

    def square_root(self, x):
        """Calculate square root."""
        return x ** 0.5
