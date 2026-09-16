import json

from fastapi import APIRouter, Query

from .db import get_connection


router = APIRouter(prefix="/api/v1/events")


@router.get("")
def get_events(
    q: str | None = None,
    container: str | None = None,
    minutes: int | None = Query(default=30, ge=1),
    limit: int = Query(default=100, ge=1, le=500),
    offset: int = Query(default=0, ge=0),
):
    conditions = []
    params = []

    if minutes is not None:
        conditions.append(
            "received_at >= NOW() - (%s * INTERVAL '1 minute')"
        )
        params.append(minutes)

    if container:
        conditions.append(
            "data #>> '{ctfwatch,container,name}' = %s"
        )
        params.append(container)

    if q:
        conditions.append("data::text ILIKE %s")
        params.append(f"%{q}%")

    where = ""
    if conditions:
        where = "WHERE " + " AND ".join(conditions)

    sql = f"""
        SELECT id, received_at, agent_id, data
        FROM events
        {where}
        ORDER BY received_at DESC
        LIMIT %s OFFSET %s
    """

    params.extend([limit, offset])

    with get_connection() as conn:
        with conn.cursor() as cur:
            cur.execute(sql, params)
            rows = cur.fetchall()

    return {
        "events": [
            {
                "id": row[0],
                "received_at": row[1],
                "agent_id": row[2],
                "data": row[3],
            }
            for row in rows
        ]
    }