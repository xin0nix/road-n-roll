import pytest
from game_api_client import Client
from game_api_client.api.default import create_game
from game_api_client.errors import UnexpectedStatus
from pycore import Server
import pytest_asyncio
import asyncio


CORE_HOST = "127.0.0.1"
CORE_PORT = 4321


@pytest_asyncio.fixture(scope="function")
async def game_server():
    """see https://docs.pytest.org/en/6.2.x/fixture.html#yield-fixtures-recommended"""
    # Setup
    srv = Server(CORE_HOST, CORE_PORT)
    srv.start()
    await asyncio.sleep(1)  # wait for server to be ready
    yield srv
    # Teardown
    captured = srv.stop()
    print(captured)


@pytest.mark.asyncio
async def test_create_game(game_server):
    client = Client(
        base_url=f"http://{CORE_HOST}:{CORE_PORT}", timeout=30.0, verify_ssl=False
    )
    async with client as client:  # assuming Client supports async context manager
        try:
            game_url = await create_game.asyncio(client=client)
            assert game_url
            assert game_url.url.startswith("/games/")
        except UnexpectedStatus as e:
            print(f"API Error: {e.status_code} - {e.content}")
            pytest.fail(f"API Error: {e.status_code} - {e.content}")
        except ConnectionError as e:
            print(f"Connection failed: {str(e)}")
            pytest.fail(f"Connection failed: {str(e)}")

    print("Server running successfully!")
