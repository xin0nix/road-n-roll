import os
from pathlib import Path


class DataBaseHandle:
    def __init__(self):
        self.db_url = self._get_db_url()
        self.base_dir = Path(__file__).parent.parent
        self.schema_dir = self.base_dir / "schema"

    def _get_db_url(self):
        host = os.getenv("DB_HOST", "localhost")
        port = os.getenv("DB_PORT", "5432")
        user = os.getenv("DB_USER", "joe")
        password = os.getenv("DB_PASSWORD", "12345678")
        dbname = os.getenv("DB_NAME", "road_n_roll")
        return f"postgresql://{user}:{password}@{host}:{port}/{dbname}"

    async def create_migrations_table(self, conn):
        query = """
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            migrated_at TIMESTAMP NOT NULL DEFAULT NOW()
        )
        """
        await conn.execute(query)

    def get_migration_files(self):
        migrations = []

        for path in sorted(self.schema_dir.iterdir()):
            assert path.is_file()
            assert path.name.endswith(".sql")
            version = int(path.name.split("_")[0])
            migrations.append(
                {
                    "version": version,
                    "name": path.name,
                    "content": path.read_text(encoding="utf-8"),
                    "path": path,
                }
            )

        return migrations
