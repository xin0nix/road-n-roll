import asyncio

from flask import Flask, render_template, redirect, request, url_for, flash
from game_api_client import Client
from game_api_client.api.default import create_game, get_game, delete_game, list_games

app = Flask(__name__)
app.secret_key = "####"  # Needed for flashing messages

CORE_HOST = "localhost"
CORE_PORT = 8080


@app.route("/")
def index():
    return redirect(url_for("games"))


@app.route("/games/<uuid:game_id>/", methods=["GET", "POST", "DELETE"])
def game_by_id(game_id):
    if request.method == "DELETE" or (
        request.method == "POST" and request.form.get("_method") == "DELETE"
    ):

        async def delete_game_async():
            client = Client(
                base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False
            )
            async with client as client:
                return await delete_game.asyncio_detailed(client=client, uuid=game_id)

        asyncio.run(delete_game_async())
        return redirect(url_for("games"))
    elif request.method == "GET":

        async def get_game_async():
            client = Client(
                base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False
            )
            async with client as client:
                return await get_game.asyncio(client=client, uuid=game_id)

        game_info = asyncio.run(get_game_async())
        game = {
            "name": str(game_id)[0:8],
            "uuid": str(game_id),
            "status": game_info["status"],
        }
        return render_template("game_with_uuid.html", game=game)


@app.route("/games/", methods=["POST", "GET"])
def games():
    if request.method == "POST":
        async def create_game_async():
            client = Client(
                base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False
            )
            async with client as client:
                return await create_game.asyncio(client=client)

        game_url = asyncio.run(create_game_async())
        if game_url and game_url.url.startswith("/games/"):
            return redirect(game_url.url)
        else:
            flash("Unexpected response from server.", "danger")
        return redirect(url_for("games"))
    elif request.method == "GET":

        async def list_games_async():
            client = Client(
                base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False
            )
            async with client as client:
                return await list_games.asyncio(client=client)

        game_list = asyncio.run(list_games_async())
        if game_list:
            games = list(
                map(
                    lambda game: {
                        "name": game.url.split("/")[-1][0:8],
                        "uuid": game.url.split("/")[-1],
                        "url": game.url,
                    },
                    game_list.games,
                )
            )
            return render_template("games.html", games=games)
        else:
            flash("Unexpected response from server.", "danger")


if __name__ == "__main__":
    app.run(debug=True)
