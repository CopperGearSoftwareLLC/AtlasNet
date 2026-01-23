
// app/layout.tsx
import type { Metadata } from "next";
import "./globals.css";
import Link from "next/link";
import {
  LayoutDashboard,
  Map,
  Server,
  Users,
  Database,
  Compass,
  Activity
} from "lucide-react";


export const metadata: Metadata = {
  title: "AtlasNet",
  description: "AtlasNet control panel",
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body className="bg-slate-950 text-slate-100">
        <div className="min-h-screen flex bg-slate-950">
          {/* Sidebar */}
          <aside className="w-40 bg-slate-950 border-r border-slate-800 flex flex-col py-6 px-4 gap-6">
            {/* Logo / brand */}
            <div className="flex items-center gap-2 px-2">

              <Compass className="h-8 w-8" />

              <div className="font-semibold text-lg">AtlasNet</div>
            </div>

            {/* Nav links */}
            <nav className="flex-1 flex flex-col gap-1 text-sm text-slate-300">
              <Link
                href="/"
                className="flex items-center gap-3 px-3 py-2 rounded-lg hover:bg-slate-900"
              >
                <LayoutDashboard className="h-4 w-4" />
                <span>Overview</span>
              </Link>
              <Link
                href="/map"
                className="flex items-center gap-3 px-3 py-2 rounded-lg hover:bg-slate-900"
              >
                <Map className="h-4 w-4" />
                <span>Map</span>
              </Link>
              <Link
                href="/players"
                className="flex items-center gap-3 px-3 py-2 rounded-lg hover:bg-slate-900"
              >
                <Users className="h-4 w-4" />
                <span>Players</span>
              </Link>
              <Link
                href="/database"
                className="flex items-center gap-3 px-3 py-2 rounded-lg hover:bg-slate-900"
              >
                <Database className="h-4 w-4" />
                <span>Database</span>
              </Link>
              <Link
                href="/workers"
                className="flex items-center gap-3 px-3 py-2 rounded-lg hover:bg-slate-900"
              >
                <Server className="h-4 w-4" />
                <span>Workers</span>
              </Link>
              <Link
                href="/network"
                className="flex items-center gap-3 px-3 py-2 rounded-lg hover:bg-slate-900"
              >
                <Server className="h-4 w-4" />
                <span>Network</span>
              </Link>
            </nav>

            <div className="px-3 text-xs text-slate-500">
              AtlasNet control panel
            </div>
          </aside>

          {/* Main content */}
          <main className="flex-1 flex flex-col overflow-hidden">
           <div className="flex-1 p-8">
    {children}
  </div>
        </main>

        </div>
      </body>
    </html>
  );
}
