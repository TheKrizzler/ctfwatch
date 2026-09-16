import os

import psycopg


DATABASE_URL = os.environ["DATABASE_URL"]


def get_connection():
    return psycopg.connect(DATABASE_URL)


def init_db():
    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute("""
                CREATE TABLE IF NOT EXISTS events (
                    id BIGSERIAL PRIMARY KEY,
                    received_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                    agent_id TEXT NOT NULL,
                    data JSONB NOT NULL
                )
            """)

            cur.execute("""
                CREATE INDEX IF NOT EXISTS events_received_at_idx
                ON events (received_at DESC)
            """)

            cur.execute("""
                CREATE INDEX IF NOT EXISTS events_data_idx
                ON events USING GIN (data)
            """)