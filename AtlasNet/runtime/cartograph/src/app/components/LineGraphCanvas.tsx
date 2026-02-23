'use client';

import React, { useEffect, useRef } from 'react';

export type LineGraphCanvasProps = {
  /** Y-values in time order (oldest → newest). */
  values: number[];
  /** Canvas width in CSS pixels. */
  width?: number;
  /** Canvas height in CSS pixels. */
  height?: number;
  /** Optional fixed Y range; if omitted, autoscale to data. */
  yMin?: number;
  yMax?: number;
};

/**
 * Simple line graph rendered to an HTML canvas.
 * Designed for telemetry time-series (ping, upload/download rates).
 */
export function LineGraphCanvas({
  values,
  width = 220,
  height = 60,
  yMin,
  yMax,
}: LineGraphCanvasProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    // HiDPI support: scale internal buffer to devicePixelRatio
    const dpr = Math.max(1, window.devicePixelRatio || 1);
    canvas.style.width = `${width}px`;
    canvas.style.height = `${height}px`;
    canvas.width = Math.floor(width * dpr);
    canvas.height = Math.floor(height * dpr);

    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0); // draw in CSS pixel coordinates

    // Clear
    ctx.clearRect(0, 0, width, height);

    // Nothing to draw
    if (values.length < 2) {
      // light baseline
      ctx.beginPath();
      ctx.moveTo(0, height - 1);
      ctx.lineTo(width, height - 1);
      ctx.strokeStyle = '#e5e7eb';
      ctx.lineWidth = 1;
      ctx.stroke();
      return;
    }

    // Compute y range
    const dataMin = Math.min(...values);
    const dataMax = Math.max(...values);
    const min = yMin ?? dataMin;
    const max = yMax ?? (dataMax === dataMin ? dataMax + 1 : dataMax);

    // Padding so line isn’t glued to edges
    const pad = 4;
    const plotW = width - pad * 2;
    const plotH = height - pad * 2;

    const xAt = (i: number) => pad + (i / (values.length - 1)) * plotW;
    const yAt = (v: number) => {
      const t = (v - min) / (max - min);
      const clamped = Math.max(0, Math.min(1, t));
      return pad + (1 - clamped) * plotH;
    };

    // Optional grid baseline
    ctx.beginPath();
    ctx.moveTo(pad, pad + plotH);
    ctx.lineTo(pad + plotW, pad + plotH);
    ctx.strokeStyle = '#e5e7eb';
    ctx.lineWidth = 1;
    ctx.stroke();

    // Line
    ctx.beginPath();
    ctx.moveTo(xAt(0), yAt(values[0]));
    for (let i = 1; i < values.length; i++) {
      ctx.lineTo(xAt(i), yAt(values[i]));
    }
    ctx.strokeStyle = '#2563eb';
    ctx.lineWidth = 2;
    ctx.stroke();
  }, [values, width, height, yMin, yMax]);

  return <canvas ref={canvasRef} />;
}
