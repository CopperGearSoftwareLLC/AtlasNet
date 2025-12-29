// app/servers/page.tsx

type ServerRow = {
  id: string;
  name: string;
  address: string;
  players: number;
  status: "Online" | "Offline";
};

const MOCK_SERVERS: ServerRow[] = [
  {
    id: "eGameServer-1",
    name: "GameServer A",
    address: "10.0.1.10:25565",
    players: 32,
    status: "Online",
  },
  {
    id: "eGameServer-2",
    name: "GameServer B",
    address: "10.0.1.11:25565",
    players: 5,
    status: "Online",
  },
  {
    id: "ePartition-7",
    name: "Partition Node 7",
    address: "10.0.2.3:4000",
    players: 80,
    status: "Online",
  },
];

export default function ServersPage() {
  return (
    <div className="space-y-6">
      {/* Header row like the screenshot */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold">Servers</h1>
          <p className="text-slate-400 text-sm mt-1">
            Current AtlasNet servers and partitions.
          </p>
        </div>

        <button className="inline-flex items-center gap-2 rounded-xl bg-indigo-500 px-4 py-2 text-sm font-medium text-white shadow hover:bg-indigo-400">
          <span className="text-lg leading-none">＋</span>
          <span>Export</span>
        </button>
      </div>

      {/* Filters row */}
      <div className="flex items-center gap-3">
        <button className="inline-flex items-center gap-2 rounded-xl bg-slate-900 border border-slate-700 px-4 py-2 text-sm text-slate-200 hover:bg-slate-800">
          <span>⚙️</span>
          <span>Filters</span>
        </button>

        <button className="inline-flex items-center gap-2 rounded-xl bg-slate-900 border border-slate-700 px-4 py-2 text-sm text-slate-200 hover:bg-slate-800">
          <span>📅</span>
          <span>Friday, February 3, 2023</span>
        </button>
      </div>

      {/* Table card */}
      <div className="mt-2 rounded-3xl bg-slate-900/80 border border-slate-800 overflow-hidden">
        {/* Table header */}
        <div className="border-b border-slate-800 px-6 py-3 text-sm font-medium text-slate-300 flex">
          <div className="w-1/3">Server</div>
          <div className="w-1/3">Address</div>
          <div className="w-1/6">Players</div>
          <div className="w-1/6 text-right">Status</div>
        </div>

        {/* Rows */}
        <div className="divide-y divide-slate-800">
          {MOCK_SERVERS.map((srv) => (
            <div key={srv.id} className="px-6 py-4 flex items-center text-sm">
              <div className="w-1/3 flex items-center gap-3">
                <div className="h-8 w-8 rounded-full bg-slate-700 flex items-center justify-center text-xs">
                  {srv.name[0]}
                </div>
                <div>
                  <div className="font-medium text-slate-100">
                    {srv.name}
                  </div>
                  <div className="text-slate-400 text-xs font-mono">
                    {srv.id}
                  </div>
                </div>
              </div>

              <div className="w-1/3 text-slate-300 font-mono text-xs">
                {srv.address}
              </div>

              <div className="w-1/6 text-slate-200">
                {srv.players} players
              </div>

              <div className="w-1/6 text-right">
                <span
                  className={
                    "inline-flex items-center rounded-full px-2 py-1 text-xs font-medium " +
                    (srv.status === "Online"
                      ? "bg-emerald-500/10 text-emerald-400"
                      : "bg-rose-500/10 text-rose-400")
                  }
                >
                  ● {srv.status}
                </span>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
