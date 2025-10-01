"""Python file with various import statements."""

import os
import sys
from pathlib import Path
from typing import List, Dict, Optional, Union
from collections import defaultdict, Counter
import json as js
from datetime import datetime, timedelta


def example_function():
    """Function using imported modules."""
    current_path = Path.cwd()
    data: Dict[str, int] = defaultdict(int)
    timestamp = datetime.now()
    return current_path, data, timestamp
