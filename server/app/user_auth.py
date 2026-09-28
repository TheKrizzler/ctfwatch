import hmac
import os
import secrets
from datetime import datetime, timedelta, timezone

from fastapi import Cookie, HTTPException, Response
from pydantic import BaseModel
from pwdlib import PasswordHash


ADMIN_USERNAME = os.environ["CTFWATCH_ADMIN_USERNAME"]
ADMIN_PASSWORD_HASH = os.environ["CTFWATCH_ADMIN_PASSWORD_HASH"]

SESSION_LIFETIME = timedelta(hours=12)

password_hash = PasswordHash.recommended()
_sessions: dict[str, datetime] = {}


class LoginRequest(BaseModel):
    username: str
    password: str


def authenticate_user(
    ctfwatch_session: str | None = Cookie(default=None),
):
    if not ctfwatch_session:
        raise HTTPException(status_code=401, detail="Unauthorized")

    expires = _sessions.get(ctfwatch_session)

    if expires is None:
        raise HTTPException(status_code=401, detail="Unauthorized")

    if datetime.now(timezone.utc) >= expires:
        _sessions.pop(ctfwatch_session, None)
        raise HTTPException(status_code=401, detail="Session expired")

    return ADMIN_USERNAME


def login(credentials: LoginRequest, response: Response):
    username_ok = hmac.compare_digest(
        credentials.username,
        ADMIN_USERNAME,
    )

    try:
        password_ok = password_hash.verify(
            credentials.password,
            ADMIN_PASSWORD_HASH,
        )
    except Exception:
        password_ok = False

    if not username_ok or not password_ok:
        raise HTTPException(
            status_code=401,
            detail="Invalid username or password",
        )

    session_id = secrets.token_urlsafe(32)

    _sessions[session_id] = (
        datetime.now(timezone.utc) + SESSION_LIFETIME
    )

    response.set_cookie(
        key="ctfwatch_session",
        value=session_id,
        httponly=True,
        secure=True,
        samesite="lax",
        max_age=int(SESSION_LIFETIME.total_seconds()),
    )

    return {"username": ADMIN_USERNAME}


def logout(
    response: Response,
    ctfwatch_session: str | None = Cookie(default=None),
):
    if ctfwatch_session:
        _sessions.pop(ctfwatch_session, None)

    response.delete_cookie(
        key="ctfwatch_session",
        secure=True,
        httponly=True,
        samesite="lax",
    )

    return {"status": "ok"}