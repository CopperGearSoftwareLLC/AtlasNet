type NativeQueryValue = string | number | boolean | null | undefined;

interface FetchNativeJsonOptions {
  path: string;
  timeoutMs: number;
  query?: Record<string, NativeQueryValue>;
}

const DEFAULT_NATIVE_BASE_URL = 'http://127.0.0.1:4000';

function getNativeBaseUrl(): string {
  const configured =
    process.env.CARTOGRAPH_NATIVE_URL ?? process.env.NATIVE_SERVER_URL;
  if (configured && configured.trim().length > 0) {
    return configured.trim().replace(/\/+$/, '');
  }
  return DEFAULT_NATIVE_BASE_URL;
}

function buildNativeUrl(path: string, query?: Record<string, NativeQueryValue>): string {
  const normalizedPath = path.startsWith('/') ? path : `/${path}`;
  const url = new URL(`${getNativeBaseUrl()}${normalizedPath}`);
  if (query) {
    for (const [key, value] of Object.entries(query)) {
      if (value == null) {
        continue;
      }
      const stringValue = String(value).trim();
      if (stringValue.length > 0) {
        url.searchParams.set(key, stringValue);
      }
    }
  }
  return url.toString();
}

export async function fetchNativeJson<T>({
  path,
  timeoutMs,
  query,
}: FetchNativeJsonOptions): Promise<T | null> {
  const ac = new AbortController();
  const timeoutId = setTimeout(() => ac.abort(), timeoutMs);

  try {
    const response = await fetch(buildNativeUrl(path, query), {
      cache: 'no-store',
      signal: ac.signal,
    });

    if (!response.ok) {
      return null;
    }

    return (await response.json()) as T;
  } catch {
    return null;
  } finally {
    clearTimeout(timeoutId);
  }
}
