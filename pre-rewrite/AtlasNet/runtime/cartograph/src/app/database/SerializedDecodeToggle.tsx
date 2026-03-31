'use client';

interface SerializedDecodeToggleProps {
  enabled: boolean;
  onChange: (enabled: boolean) => void;
}

export function SerializedDecodeToggle({
  enabled,
  onChange,
}: SerializedDecodeToggleProps) {
  return (
    <label className="flex min-w-60 items-center gap-2 text-xs text-slate-400">
      <input
        type="checkbox"
        checked={enabled}
        onChange={(e) => onChange(e.target.checked)}
      />
      decode byte optimization [experimental]
    </label>
  );
}
