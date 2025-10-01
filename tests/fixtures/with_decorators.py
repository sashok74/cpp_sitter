"""Python file with decorators for testing."""

from functools import wraps
import time


def timer(func):
    """Decorator to measure execution time."""
    @wraps(func)
    def wrapper(*args, **kwargs):
        start = time.time()
        result = func(*args, **kwargs)
        end = time.time()
        print(f"{func.__name__} took {end - start:.4f} seconds")
        return result
    return wrapper


def retry(max_attempts=3):
    """Decorator to retry function on failure."""
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            for attempt in range(max_attempts):
                try:
                    return func(*args, **kwargs)
                except Exception as e:
                    if attempt == max_attempts - 1:
                        raise
                    print(f"Attempt {attempt + 1} failed: {e}")
            return None
        return wrapper
    return decorator


class DataProcessor:
    """Class with decorated methods."""

    @staticmethod
    @timer
    def process_data(data):
        """Process data with timing."""
        return [x * 2 for x in data]

    @classmethod
    @retry(max_attempts=5)
    def load_config(cls, filename):
        """Load configuration with retry."""
        with open(filename) as f:
            return f.read()

    @property
    def status(self):
        """Get processing status."""
        return "ready"


@timer
@retry(max_attempts=3)
def complex_operation(x, y):
    """Function with multiple decorators."""
    return x ** y
