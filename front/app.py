import asyncio

from flask import Flask, render_template, redirect, url_for, flash
from game_api_client import Client
from game_api_client.api.default import create_game, get_game, list_games

app = Flask(__name__)
app.secret_key = "####"  # Needed for flashing messages

CORE_HOST = "localhost"
CORE_PORT = 8080


@app.route("/")
def index():
    return redirect(url_for("games"))


@app.route("/games/<uuid:game_id>/")
def game_with_uuid(game_id):
    async def get_game_async():
        client = Client(base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False)
        async with client as client:
            return await get_game.asyncio(client=client, uuid=game_id)

    game_info = asyncio.run(get_game_async())
    return render_template("game_with_uuid.html", game_info=game_info)


@app.post("/games")
def create_game_route():
    async def create_game_async():
        client = Client(base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False)
        async with client as client:
            return await create_game.asyncio(client=client)

    game_url = asyncio.run(create_game_async())
    if game_url and game_url.url.startswith("/games/"):
        return redirect(game_url.url)
    else:
        flash("Unexpected response from server.", "danger")
    return redirect(url_for("games"))


@app.get("/games")
def get_games_route():
    async def list_games_async():
        client = Client(base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False)
        async with client as client:
            return await list_games.asyncio(client=client)

    game_list = asyncio.run(list_games_async())
    if game_list:
        print(game_list)
        return render_template("games.html", game_list=game_list)
    else:
        flash("Unexpected response from server.", "danger")


if __name__ == "__main__":
    app.run(debug=True)
