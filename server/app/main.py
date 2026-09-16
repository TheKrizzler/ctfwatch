import gzip
import json
from contextlib import asynccontextmanager

from fastapi import FastAPI, HTTPException, Request
from fastapi.staticfiles import StaticFiles

from .db import get_connection, init_db
from .events import router as events_router


@asynccontextmanager
async def lifespan(app: FastAPI):
    init_db()
    yield


app = FastAPI(
    title="CTFWatch",
    lifespan=lifespan
)


@app.get("/api/v1/health")
def health():
    return {"status": "ok"}


@app.post("/api/v1/logs/ingest")
async def ingest_logs(request: Request):
    body = await request.body()

    if request.headers.get("content-encoding") == "gzip":
        try:
            body = gzip.decompress(body)
        except gzip.BadGzipFile:
            raise HTTPException(
                status_code=400,
                detail="Invalid gzip body"
            )

    try:
        payload = json.loads(body)
    except (json.JSONDecodeError, UnicodeDecodeError):
        raise HTTPException(
            status_code=400,
            detail="Invalid JSON"
        )

    events = payload.get("events")

    if not isinstance(events, list):
        raise HTTPException(
            status_code=400,
            detail="Expected events array"
        )

    if not events:
        return {"accepted": 0}

    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.executemany(
                """
                INSERT INTO events (agent_id, data)
                VALUES (%s, %s)
                """,
                [
                    ("development", json.dumps(event))
                    for event in events
                ]
            )

    return {"accepted": len(events)}

app.include_router(events_router)

app.mount(
    "/",
    StaticFiles(
        directory="/app/frontend",
        html=True
    ),
    name="frontend"
)