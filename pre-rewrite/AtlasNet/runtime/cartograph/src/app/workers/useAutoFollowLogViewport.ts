import { useCallback, useEffect, useRef, type UIEvent } from 'react';

const LOG_VIEWPORT_BOTTOM_EPSILON_PX = 24;

function isScrolledToBottom(viewport: HTMLElement): boolean {
  const distanceFromBottom =
    viewport.scrollHeight - viewport.scrollTop - viewport.clientHeight;
  return distanceFromBottom <= LOG_VIEWPORT_BOTTOM_EPSILON_PX;
}

export function useAutoFollowLogViewport(visibleLogs: string, resetToken: string) {
  const logsViewportRef = useRef<HTMLPreElement | null>(null);
  const shouldAutoScrollLogsRef = useRef(true);
  const lastVisibleLogsRef = useRef('');

  useEffect(() => {
    shouldAutoScrollLogsRef.current = true;
    lastVisibleLogsRef.current = '';
  }, [resetToken]);

  useEffect(() => {
    const previous = lastVisibleLogsRef.current;
    const changed = previous !== visibleLogs;
    lastVisibleLogsRef.current = visibleLogs;

    if (!changed || !shouldAutoScrollLogsRef.current) {
      return;
    }

    const viewport = logsViewportRef.current;
    if (!viewport) {
      return;
    }
    viewport.scrollTop = viewport.scrollHeight;
  }, [visibleLogs]);

  const onLogsScroll = useCallback((event: UIEvent<HTMLPreElement>) => {
    shouldAutoScrollLogsRef.current = isScrolledToBottom(event.currentTarget);
  }, []);

  return {
    logsViewportRef,
    onLogsScroll,
  };
}
