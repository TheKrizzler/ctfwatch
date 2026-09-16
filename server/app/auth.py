import hashlib

from fastapi import Header, HTTPException

from .db import get_connection


def hash_token(token: str) -> str:
    return hashlib.sha256(token.encode()).hexdigest()


def authenticate_agent(
    authorization: str | None = Header(default=None),
):
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(
            status_code=401,
            detail="Unauthorized",
        )

    token = authorization[7:]

    if not token:
        raise HTTPException(
            status_code=401,
            detail="Unauthorized",
        )

    token_hash = hash_token(token)

    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute(
                """
                SELECT id, name
                FROM agents
                WHERE token_hash = %s
                """,
                (token_hash,),
            )
            row = cur.fetchone()

    if not row:
        raise HTTPException(
            status_code=401,
            detail="Unauthorized",
        )

    return {
        "id": row[0],
        "name": row[1],
    }