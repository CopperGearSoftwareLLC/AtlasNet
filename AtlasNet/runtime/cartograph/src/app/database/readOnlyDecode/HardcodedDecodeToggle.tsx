'use client';

interface HardcodedDecodeToggleProps {
  enabled: boolean;
  onChange: (enabled: boolean) => void;
}

export function HardcodedDecodeToggle({
  enabled,
  onChange,
}: HardcodedDecodeToggleProps) {
  return (
    <label className="flex min-w-60 items-center gap-2 text-xs text-slate-400">
      <input
        type="checkbox"
        checked={enabled}
        onChange={(e) => onChange(e.target.checked)}
      />
      revert byte optimization [experimental]
    </label>
  );
}
