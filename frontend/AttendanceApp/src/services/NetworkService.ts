import NetInfo from '@react-native-community/netinfo';

export class NetworkService {
  private isOnline: boolean = false;
  private listeners: ((isOnline: boolean) => void)[] = [];

  async initialize() {
    const state = await NetInfo.fetch();
    this.isOnline = state.isConnected ?? false;
    console.log(`Network state: ${this.isOnline ? 'ONLINE' : 'OFFLINE'}`);

    // Subscribe to network state changes
    NetInfo.addEventListener((state) => {
      const wasOnline = this.isOnline;
      this.isOnline = state.isConnected ?? false;

      if (wasOnline !== this.isOnline) {
        console.log(
          `Network state changed: ${this.isOnline ? 'ONLINE' : 'OFFLINE'}`
        );
        this.notifyListeners();
      }
    });
  }

  isConnected(): boolean {
    return this.isOnline;
  }

  subscribe(callback: (isOnline: boolean) => void): () => void {
    this.listeners.push(callback);

    // Return unsubscribe function
    return () => {
      this.listeners = this.listeners.filter((listener) => listener !== callback);
    };
  }

  private notifyListeners() {
    this.listeners.forEach((listener) => listener(this.isOnline));
  }
}

export const networkService = new NetworkService();
