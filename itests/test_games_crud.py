import pytest
from pycore import Server, Client, Requests, Errors


@pytest.fixture(scope="module")
async def api_server():
    srv = Server()
    srv.start()
    # Optionally wait a bit for server to be ready, e.g. asyncio.sleep(1)
    yield srv
    srv.stop()


@pytest.mark.asyncio
async def test_game_api(api_server):
    client = Client(base_url="http://127.0.0.1:8080", timeout=30.0, verify_ssl=False)
    print(f"Server started: {api_server}")
    with client as client:
        try:
            list_response = await Requests["create_game"].asyncio(client=client)
            print(list_response)
        except Errors.UnexpectedStatus as e:
            print(f"API Error: {e.status_code} - {e.content}")
            pytest.fail(f"API Error: {e.status_code} - {e.content}")
        except ConnectionError as e:
            print(f"Connection failed: {str(e)}")
            pytest.fail(f"Connection failed: {str(e)}")

    print("Server running successfully!")
