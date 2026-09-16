import os
import psycopg

DATABASE_URL = os.environ["DATABASE_URL"]


def get_connection():
    return psycopg.connect(DATABASE_URL)


def init_db():
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute("""
                CREATE TABLE IF NOT EXISTS agents (
                    id BIGSERIAL PRIMARY KEY,
                    name TEXT NOT NULL UNIQUE,
                    token_hash TEXT NOT NULL UNIQUE,
                    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
                )
            """)

            cur.execute("""
                CREATE TABLE IF NOT EXISTS events (
                    id BIGSERIAL PRIMARY KEY,
                    received_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                    agent_id BIGINT NOT NULL REFERENCES agents(id),
                    data JSONB NOT NULL
                )
            """)

            cur.execute("""
                CREATE INDEX IF NOT EXISTS events_received_at_idx
                ON events (received_at DESC)
            """)

            cur.execute("""
                CREATE INDEX IF NOT EXISTS events_agent_id_idx
                ON events (agent_id)
            """)

            cur.execute("""
                CREATE INDEX IF NOT EXISTS events_data_idx
                ON events USING GIN (data)
            """)