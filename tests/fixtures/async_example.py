"""Python async/await examples for testing."""

import asyncio
from typing import List


async def fetch_data(url: str) -> str:
    """Asynchronously fetch data from URL."""
    await asyncio.sleep(0.1)  # Simulate network delay
    return f"Data from {url}"


async def process_item(item: dict) -> dict:
    """Process a single item asynchronously."""
    await asyncio.sleep(0.05)
    return {"processed": item}


async def batch_process(items: List[dict]) -> List[dict]:
    """Process multiple items concurrently."""
    tasks = [process_item(item) for item in items]
    results = await asyncio.gather(*tasks)
    return results


class AsyncDataFetcher:
    """Async data fetcher class."""

    def __init__(self, base_url: str):
        """Initialize fetcher with base URL."""
        self.base_url = base_url
        self.cache = {}

    async def get(self, endpoint: str) -> str:
        """Get data from endpoint."""
        if endpoint in self.cache:
            return self.cache[endpoint]

        url = f"{self.base_url}/{endpoint}"
        data = await fetch_data(url)
        self.cache[endpoint] = data
        return data

    async def get_multiple(self, endpoints: List[str]) -> List[str]:
        """Get data from multiple endpoints."""
        tasks = [self.get(ep) for ep in endpoints]
        return await asyncio.gather(*tasks)

    async def __aenter__(self):
        """Async context manager entry."""
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Async context manager exit."""
        self.cache.clear()


async def main():
    """Main async function."""
    async with AsyncDataFetcher("https://api.example.com") as fetcher:
        data = await fetcher.get("users")
        print(data)


if __name__ == "__main__":
    asyncio.run(main())
