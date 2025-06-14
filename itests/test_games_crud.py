"""
Модуль для тестирования API игрового сервера с использованием pytest и asyncio.

В модуле реализовано:
- Асинхронная фикстура pytest для запуска и остановки игрового сервера, написанного на C++ (обёртка pycore.Server).
- Асинхронный тест, который с помощью сгенерированного клиента game_api_client вызывает API создания игры.
- Обработка ошибок при неожиданных ответах сервера и проблемах с подключением.

Требования:
- Python 3.12 и выше
- pytest, pytest-asyncio
- game_api_client (сгенерированный из OpenAPI спецификации)
- pycore (обёртка для C++ игрового сервера)
- asyncio — стандартная библиотека Python для работы с асинхронным кодом

Сценарий работы теста:
1. Перед тестом запускается игровой сервер.
2. Через асинхронный клиент отправляется запросы на работу с играми.
3. Проверяется, что сервер вернул корректные данные в ответе.
4. После теста сервер останавливается, а логи выводятся в консоль.
"""

import asyncio
import uuid
import pytest_asyncio
import pytest
import asyncpg
import os

from game_api_client import Client
from game_api_client.api.default import create_game, get_game, delete_game, list_games
from game_api_client.errors import UnexpectedStatus
from pycore import Server


CORE_HOST = "127.0.0.1"
CORE_PORT = 4321


def get_db_url():
    host = os.getenv("DB_HOST", "localhost")
    port = os.getenv("DB_PORT", "5432")
    user = os.getenv("DB_USER", "joe")
    password = os.getenv("DB_PASSWORD", "12345678")
    dbname = os.getenv("DB_NAME", "road_n_roll")
    return f"postgresql://{user}:{password}@{host}:{port}/{dbname}"


@pytest_asyncio.fixture(scope="function")
async def game_server():
    """
    Асинхронная фикстура для запуска и остановки игрового сервера для каждого теста.

    Setup:
        - Запускает сервер на C++ через обёртку pycore.Server.
        - Асинхронно ждёт 1 секунду, чтобы сервер успел полностью стартовать.

    Yield:
        - Передаёт запущенный сервер тесту.

    Teardown:
        - Останавливает сервер и выводит собранные логи.
    """
    srv = Server(CORE_HOST, CORE_PORT)
    srv.start()
    await asyncio.sleep(1)
    yield srv
    captured = srv.stop()
    print(captured)


@pytest_asyncio.fixture(autouse=True)
async def cleanup_games_table():
    yield
    conn = await asyncpg.connect(get_db_url())
    try:
        await conn.execute("TRUNCATE TABLE GAMES RESTART IDENTITY CASCADE;")
    finally:
        await conn.close()


@pytest.mark.asyncio
async def test_post_game(game_server):
    """
    Тест создания новой игры через API игрового сервера.

    Аргументы:
        game_server: запущенный игровой сервер из фикстуры.

    Шаги теста:
        - Создаёт асинхронного клиента, направленного на локальный сервер.
        - Асинхронно вызывает эндпоинт create_game.
        - Проверяет, что в ответе есть URL игры и он начинается с "/games/".
        - Обрабатывает и выводит ошибки API и проблемы с подключением.

    Подробнее про pytest-asyncio и асинхронное тестирование можно прочитать здесь:
    https://habr.com/ru/companies/otus/articles/337108/
    """
    client = Client(base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False)
    async with client as client:
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


@pytest.mark.asyncio
async def test_post_and_get_game(game_server):
    """
    Тест создания новой игры и получения информации о ней через API игрового сервера.

    Аргументы:
        game_server: запущенный игровой сервер из фикстуры.

    Шаги теста:
        - Отправляется запрос на создание новой игры (create_game).
        - Проверяется, что игра успешно создана и получен URL с UUID игры.
        - Извлекается UUID игры из URL.
        - Отправляется запрос на получение информации об игре по UUID (get_game).
        - Проверяется, что информация об игре получена и статус игры — "Активна".
        - Проверяется, что URL игры в ответе совпадает с URL созданной игры.

    Подробнее про pytest-asyncio и асинхронное тестирование можно прочитать здесь:
    https://habr.com/ru/companies/otus/articles/337108/
    """
    client = Client(base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False)
    async with client as client:
        try:
            posted_game = await create_game.asyncio(client=client)
            assert posted_game
            game_uuid = uuid.UUID(posted_game.url.split("/games/")[-1])
            game_info = await get_game.asyncio(client=client, uuid=game_uuid)
            assert game_info
            assert game_info["status"] == "pending"
            assert game_info.url == posted_game.url
        except UnexpectedStatus as e:
            print(f"API Error: {e.status_code} - {e.content}")
            pytest.fail(f"API Error: {e.status_code} - {e.content}")
        except ConnectionError as e:
            print(f"Connection failed: {str(e)}")
            pytest.fail(f"Connection failed: {str(e)}")

    print("Server running successfully!")


@pytest.mark.asyncio
async def test_post_and_delete_game(game_server):
    """
    Тест создания новой игры и её удаления через API игрового сервера.

    Аргументы:
        game_server: запущенный игровой сервер из фикстуры.

    Шаги теста:
        - Отправляется запрос на создание новой игры (create_game).
        - Проверяется, что игра успешно создана и получен URL с UUID игры.
        - Извлекается UUID игры из URL.
        - Отправляется запрос на удаление игры по UUID (delete_game).
        - Проверяется, что сервер вернул статус 204 No Content, подтверждающий успешное удаление.
        - Пытаемся получить информацию об удалённой игре (get_game).
        - Проверяется, что сервер возвращает статус 404 Not Found, что подтверждает удаление игры.
        - Обрабатываются и выводятся ошибки API и проблемы с подключением.

    Подробнее про pytest-asyncio и асинхронное тестирование можно прочитать здесь:
    https://habr.com/ru/companies/otus/articles/337108/
    """
    client = Client(base_url=f"http://{CORE_HOST}:{CORE_PORT}", verify_ssl=False)
    async with client as client:
        try:
            posted_game = await create_game.asyncio(client=client)
            assert posted_game
            game_uuid = uuid.UUID(posted_game.url.split("/games/")[-1])
            deleted = await delete_game.asyncio_detailed(client=client, uuid=game_uuid)
            assert deleted.status_code == 204
            deleted = await delete_game.asyncio_detailed(client=client, uuid=game_uuid)
            assert deleted.status_code == 404
            game_info = await get_game.asyncio_detailed(client=client, uuid=game_uuid)
            assert game_info.status_code == 404
        except UnexpectedStatus as e:
            print(f"API Error: {e.status_code} - {e.content}")
            pytest.fail(f"API Error: {e.status_code} - {e.content}")
        except ConnectionError as e:
            print(f"Connection failed: {str(e)}")
            pytest.fail(f"Connection failed: {str(e)}")

    print("Server running successfully!")


@pytest.mark.asyncio
async def test_create_three_games_and_list(game_server):
    """
    Тест создания трёх игр и получения их списка через API.

    Шаги теста:
        - Создать три игры последовательно через create_game.
        - Вызвать list_games для получения списка всех игр.
        - Проверить, что в списке присутствуют все созданные игры.
        - Проверить корректность данных каждой игры в списке.
    """
    pass


@pytest.mark.asyncio
async def test_create_and_delete_games_sequentially_check_list(game_server):
    """
    Тест создания трёх игр, поочерёдного удаления каждой с проверкой списка игр.

    Шаги теста:
        - Создать три игры через create_game.
        - Удалять игры по одной через delete_game.
        - После каждого удаления вызывать list_games.
        - Проверять, что удалённая игра отсутствует в списке, а остальные присутствуют.
    """
    # Испльзуй endpoint ниже, по аналогии (можно кликнуть и глянуть код)
    _ = list_games
    pass


@pytest.mark.asyncio
async def test_create_delete_create_check_old_game_unavailable(game_server):
    """
    Тест создания игры, её удаления, создания новой и проверки недоступности старой.

    Шаги теста:
        - Создать игру через create_game.
        - Удалить эту игру через delete_game.
        - Создать новую игру.
        - Проверить, что старая игра недоступна через get_game (ожидается 404).
        - Проверить, что новая игра доступна и корректна.
    """
    pass


@pytest.mark.asyncio
async def test_create_delete_create_delete_check(game_server):
    """
    Тест создания игры, её удаления, создания новой игры и удаления новой.

    Шаги теста:
        - Создать игру через create_game.
        - Удалить игру через delete_game.
        - Создать новую игру.
        - Удалить новую игру.
        - Проверить, что обе игры недоступны через get_game (ожидается 404).
    """
    pass
