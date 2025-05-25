import asyncio

from flask import Flask, render_template, redirect, request, url_for, flash
from game_api_client import Client
from game_api_client.api.default import create_game, list_games
from game_api_client.errors import UnexpectedStatus

app = Flask(__name__)
app.secret_key = "####"  # Needed for flashing messages

CORE_HOST = "localhost"
CORE_PORT = 8080


@app.route("/")
def index():
    return redirect(url_for("games"))


@app.route("/games/<uuid:game_id>/")
def game_with_uuid(game_id):
    print(game_id)
    return render_template("game_with_uuid.html")


@app.post("/games")
def create_game_route():
    async def create_game_async():
        client = Client(base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False)
        async with client as client:
            return await create_game.asyncio(client=client)

    try:
        game_url = asyncio.run(create_game_async())
        return redirect(url_for("get_games_route"))
        # if game_url and game_url.url.startswith("/games/"):
        #     return redirect(game_url.url)
        # else:
        #     flash("Unexpected response from server.", "danger")
    except UnexpectedStatus as e:
        flash(f"API Error: {e.status_code} - {e.content}", "danger")
    except ConnectionError as e:
        flash(f"Connection failed: {str(e)}", "danger")
    except Exception as e:
        flash(f"Unexpected error: {str(e)}", "danger")

    return redirect(url_for("games"))


@app.get("/games")
def get_games_route():
    async def list_games_async():
        client = Client(base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False)
        async with client as client:
            return await list_games.asyncio(client=client)

    try:
        game_list = asyncio.run(list_games_async())
        if game_list:
            print(game_list)
            return render_template("games.html", game_list=game_list)
        else:
            flash("Unexpected response from server.", "danger")
    except UnexpectedStatus as e:
        flash(f"API Error: {e.status_code} - {e.content}", "danger")
    except ConnectionError as e:
        flash(f"Connection failed: {str(e)}", "danger")
    except Exception as e:
        flash(f"Unexpected error: {str(e)}", "danger")

    # return redirect(url_for("games"))


if __name__ == "__main__":
    app.run(debug=True)
