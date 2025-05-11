from .server import Server
from .api.game_api_client import types as Types
from .api.game_api_client import errors as Errors
from .api.game_api_client.client import Client
from .api.game_api_client.models import delete_game_response_204, game_list, game_url
from .api.game_api_client.api.default import (
    create_game,
    delete_game,
    get_game,
    list_games,
)

Models = {
    "delete_game_response_204": delete_game_response_204,
    "game_list": game_list,
    "game_url": game_url,
}

Requests = {
    "create_game": create_game,
    "delete_game": delete_game,
    "get_game": get_game,
    "list_games": list_games,
}

__all__ = ["Client", "Errors", "Models", "Requests", "Server", "Types"]

__version__ = "0.0.1"
