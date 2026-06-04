import NetInfo, { NetInfoState } from '@react-native-community/netinfo';

type NetworkCallback = (online: boolean) => void;

class NetworkService {
  private online = true;
  private idle = false;
  private idleTimer: ReturnType<typeof setTimeout> | null = null;
  private listeners: NetworkCallback[] = [];
  private unsubscribe: (() => void) | null = null;

  constructor() {
    this.unsubscribe = NetInfo.addEventListener((state: NetInfoState) => {
      const wasOnline = this.online;
      this.online = !!(state.isConnected && state.isInternetReachable !== false);
      if (wasOnline !== this.online) {
        this.listeners.forEach((cb) => cb(this.online));
      }
    });
  }

  get isOnline(): boolean {
    return this.online;
  }

  get isIdle(): boolean {
    return this.idle;
  }

  markActive(): void {
    this.idle = false;
    if (this.idleTimer) clearTimeout(this.idleTimer);
    this.idleTimer = setTimeout(() => {
      this.idle = true;
    }, 30000);
  }

  onNetworkChange(callback: NetworkCallback): () => void {
    this.listeners.push(callback);
    return () => {
      this.listeners = this.listeners.filter((cb) => cb !== callback);
    };
  }

  async checkConnection(): Promise<boolean> {
    const state = await NetInfo.fetch();
    this.online = !!(state.isConnected && state.isInternetReachable !== false);
    return this.online;
  }

  cleanup(): void {
    if (this.unsubscribe) this.unsubscribe();
    if (this.idleTimer) clearTimeout(this.idleTimer);
  }
}

export const networkService = new NetworkService();
