import { useEffect, useState } from "react";

export default function App() {
    const [events, setEvents] = useState([]);
    const [query, setQuery] = useState("");
    const [container, setContainer] = useState("");
    const [minutes, setMinutes] = useState(30);
    const [selected, setSelected] = useState(null);
    const [loading, setLoading] = useState(false);

    async function loadEvents() {
        setLoading(true);

        const params = new URLSearchParams({
            minutes: String(minutes),
            limit: "200",
        });

        if (query)
            params.set("q", query);

        if (container)
            params.set("container", container);

        try {
            const response = await fetch(
                `/api/v1/events?${params}`
            );

            const body = await response.json();
            setEvents(body.events ?? []);
        } finally {
            setLoading(false);
        }
    }

    useEffect(() => {
        loadEvents();
    }, [minutes, container]);

    function submit(event) {
        event.preventDefault();
        loadEvents();
    }

    const containers = [
        ...new Set(
            events
                .map(e => e.data.ctfwatch?.container?.name)
                .filter(Boolean)
        )
    ];

    function summary(data) {
        const method = data.http?.request?.method;

        if (method)
            return method;

        if (data.event?.action)
            return data.event.action;

        if (data.message)
            return data.message;

        return "—";
    }

    return (
        <main>
            <header>
                <div>
                    <h1>CTFWatch</h1>
                    <span>Log Explorer</span>
                </div>

                <button onClick={loadEvents}>
                    {loading ? "Loading..." : "Refresh"}
                </button>
            </header>

            <form className="toolbar" onSubmit={submit}>
                <input
                    type="search"
                    placeholder="Search logs..."
                    value={query}
                    onChange={e => setQuery(e.target.value)}
                />

                <select
                    value={container}
                    onChange={e => setContainer(e.target.value)}
                >
                    <option value="">All containers</option>

                    {containers.map(name => (
                        <option key={name} value={name}>
                            {name}
                        </option>
                    ))}
                </select>

                <select
                    value={minutes}
                    onChange={e => setMinutes(Number(e.target.value))}
                >
                    <option value="5">Last 5 minutes</option>
                    <option value="30">Last 30 minutes</option>
                    <option value="60">Last hour</option>
                    <option value="360">Last 6 hours</option>
                    <option value="1440">Last 24 hours</option>
                    <option value="999999">All time</option>
                </select>

                <button type="submit">
                    Search
                </button>
            </form>

            <section className="events">
                <div className="event event-header">
                    <span>Time</span>
                    <span>Container</span>
                    <span>Event</span>
                    <span>Source</span>
                    <span>Destination</span>
                </div>

                {events.map(event => {
                    const data = event.data;
                    const containerName =
                        data.ctfwatch?.container?.name ?? "—";

                    return (
                        <div key={event.id}>
                            <div
                                className="event event-row"
                                onClick={() =>
                                    setSelected(
                                        selected === event.id
                                            ? null
                                            : event.id
                                    )
                                }
                            >
                                <span>
                                    {new Date(
                                        event.received_at
                                    ).toLocaleTimeString()}
                                </span>

                                <span>{containerName}</span>
                                <span>{summary(data)}</span>
                                <span>{data.source?.ip ?? "—"}</span>
                                <span>{data.destination?.ip ?? "—"}</span>
                            </div>

                            {selected === event.id && (
                                <pre>
                                    {JSON.stringify(data, null, 2)}
                                </pre>
                            )}
                        </div>
                    );
                })}

                {!loading && events.length === 0 && (
                    <div className="empty">
                        No events found.
                    </div>
                )}
            </section>
        </main>
    );
}