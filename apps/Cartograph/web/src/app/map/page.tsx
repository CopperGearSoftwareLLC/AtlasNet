"use client";
import { useEffect, useRef } from "react";

export default function MapPage() {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);

  useEffect(() => {
    const canvas = canvasRef.current!;
    (window as any).Module = { canvas, locateFile: (p: string) => `/wasm/${p}` };

    const s = document.createElement("script");
    s.type = "module";
    s.textContent = `
      import Mod from "/wasm/CartographViewer.js";
      Mod({ canvas: document.getElementById("canvas"),
            locateFile: (p) => "/wasm/" + p });
    `;
    document.body.appendChild(s);

    return () => { document.body.removeChild(s); };
  }, []);

  return <canvas id="canvas" ref={canvasRef} className="w-full h-[70vh] block bg-black" />;
}