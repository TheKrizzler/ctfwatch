import secrets
import sys

from .auth import hash_token
from .db import get_connection


def main():
    if len(sys.argv) != 2:
        print(f"Usage: python -m app.create_agent <name>")
        raise SystemExit(1)

    name = sys.argv[1]

    token = secrets.token_urlsafe(32)
    token_hash = hash_token(token)

    try:
        with get_connection() as conn:
            with conn.cursor() as cur:
                cur.execute(
                    """
                    INSERT INTO agents (name, token_hash)
                    VALUES (%s, %s)
                    RETURNING id
                    """,
                    (name, token_hash),
                )

                agent_id = cur.fetchone()[0]

    except Exception as exc:
        print(f"Failed to create agent: {exc}")
        raise SystemExit(1)

    print(f"Created agent {name!r} (id={agent_id})")
    print()
    print("Token:")
    print(token)
    print()
    print("This token will not be shown again.")


if __name__ == "__main__":
    main()