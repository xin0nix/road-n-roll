import argparse
import asyncio
import sys
import asyncpg

from handle import DataBaseHandle


class DatabaseMigrator(DataBaseHandle):
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

    migrator = DatabaseMigrator()
    asyncio.run(migrator.run_migrations(args.dry_run))
