import argparse
import asyncio
import asyncpg
import json
import os
import sys
import yaml
from pathlib import Path

from jsonschema import validate


class DataBaseHandle:
    def __init__(self):
        self.db_url = self._get_db_url()
        self.base_dir = Path(__file__).parent.parent

    def _get_db_url(self):
        host = os.getenv("DB_HOST", "localhost")
        port = os.getenv("DB_PORT", "5432")
        user = os.getenv("DB_USER", "joe")
        password = os.getenv("DB_PASSWORD", "12345678")
        dbname = os.getenv("DB_NAME", "road_n_roll")
        return f"postgresql://{user}:{password}@{host}:{port}/{dbname}"


class DataMigrator(DataBaseHandle):
    def __init__(self):
        super().__init__()
        with open(self.base_dir / "data" / "data.yaml", encoding="utf-8") as f:
            self.data = yaml.safe_load(f)
        with open(self.base_dir / "data" / "schema.json", encoding="utf-8") as f:
            self.schema = json.load(f)
        validate(instance=self.data, schema=self.schema)


class SchemaMigrator(DataBaseHandle):
    def __init__(self):
        super().__init__()
        self.schema_dir = self.base_dir / "schema"

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

    async def get_applied_versions(self, conn):
        await self.create_migrations_table(conn)
        return {
            row["version"]
            for row in await conn.fetch("SELECT version FROM schema_migrations")
        }

    async def apply_migrations(self, conn, pending_migrations, dry_run=False):
        if not pending_migrations:
            print("✅ No pending migrations. Database is up-to-date.")
            return

        print(f"🚀 Found {len(pending_migrations)} pending migration(s):")
        for mig in pending_migrations:
            print(f"  - {mig['name']}")

        if dry_run:
            print("🌵 Dry run complete. No changes applied.")
            return

        async with conn.transaction():
            for mig in pending_migrations:
                print(f"📦 Applying migration: {mig['name']}")
                await conn.execute(mig["content"])
                await conn.execute(
                    "INSERT INTO schema_migrations (version, name) VALUES ($1, $2)",
                    mig["version"],
                    mig["name"],
                )
                print(f"✅ Successfully applied: {mig['name']}")

    async def run_migrations(self, dry_run=False):
        try:
            conn = await asyncpg.connect(self.db_url)

            # Get migration state
            applied_versions = await self.get_applied_versions(conn)
            all_migrations = self.get_migration_files()

            # Find pending migrations
            pending = [
                m for m in all_migrations if m["version"] not in applied_versions
            ]

            # Apply migrations
            await self.apply_migrations(conn, pending, dry_run)

            await conn.close()
            print("✨ Migration process completed successfully!")

        except Exception as e:
            print(f"❌ Migration failed: {e}")
            sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Database migrator")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Simulate migration without applying changes",
    )
    args = parser.parse_args()

    schema_migrator = SchemaMigrator()
    asyncio.run(schema_migrator.run_migrations(args.dry_run))
