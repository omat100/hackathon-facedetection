import React, { useState, useCallback } from 'react';
import { StyleSheet, Text, View, FlatList, TouchableOpacity } from 'react-native';
import { useFocusEffect } from 'expo-router';
import { DatabaseService, AttendanceRecord } from '../../services/DatabaseService';
import { syncService } from '../../services/SyncService';

export default function HomeScreen() {
  const [records, setRecords] = useState<AttendanceRecord[]>([]);
  const [stats, setStats] = useState({ total: 0, synced: 0, unsynced: 0 });

  const loadData = useCallback(async () => {
    try {
      const [fetched, s] = await Promise.all([
        DatabaseService.getAttendance(50),
        DatabaseService.getStats(),
      ]);
      setRecords(fetched);
      setStats(s);
    } catch (e) {
      console.error('Failed to load attendance data:', e);
    }
  }, []);

  useFocusEffect(
    useCallback(() => {
      loadData();
    }, [loadData])
  );

  const handleSyncNow = async () => {
    try {
      await syncService.syncAttendance();
      await loadData();
    } catch (e) {
      console.error('Sync failed:', e);
    }
  };

  const renderItem = ({ item }: { item: AttendanceRecord }) => (
    <View style={styles.card}>
      <View style={styles.avatar}>
        <Text style={styles.avatarText}>
          {(item.person_name || item.person_id)[0]?.toUpperCase() ?? '?'}
        </Text>
      </View>
      <View style={styles.info}>
        <Text style={styles.nameText}>{item.person_name || item.person_id}</Text>
        <Text style={styles.timeText}>
          {new Date(item.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
        </Text>
      </View>
      <View style={styles.rightCol}>
        <View style={[styles.statusPill, item.synced ? styles.syncedPill : styles.unsyncedPill]}>
          <Text style={[styles.statusText, item.synced ? styles.syncedText : styles.unsyncedText]}>
            {item.synced ? 'Synced' : 'Pending'}
          </Text>
        </View>
        <Text style={styles.confidenceText}>{(item.confidence * 100).toFixed(0)}%</Text>
      </View>
    </View>
  );

  return (
    <View style={styles.container}>
      <View style={styles.statsBar}>
        <View style={styles.stat}>
          <Text style={styles.statValue}>{stats.total}</Text>
          <Text style={styles.statLabel}>Total</Text>
        </View>
        <View style={styles.stat}>
          <Text style={styles.statValue}>{stats.synced}</Text>
          <Text style={styles.statLabel}>Synced</Text>
        </View>
        <View style={styles.stat}>
          <Text style={[styles.statValue, stats.unsynced > 0 && { color: '#FF6B6B' }]}>
            {stats.unsynced}
          </Text>
          <Text style={styles.statLabel}>Pending</Text>
        </View>
        <TouchableOpacity style={styles.syncButton} onPress={handleSyncNow}>
          <Text style={styles.syncButtonText}>Sync</Text>
        </TouchableOpacity>
      </View>

      <FlatList
        data={records}
        keyExtractor={(item) => item.id.toString()}
        contentContainerStyle={styles.listContainer}
        ListEmptyComponent={
          <View style={styles.emptyContainer}>
            <Text style={styles.emptyTitle}>No Attendance Yet</Text>
            <Text style={styles.emptySubtitle}>Go to Mark Attendance tab to start</Text>
          </View>
        }
        renderItem={renderItem}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#f5f5f5' },
  statsBar: {
    flexDirection: 'row',
    backgroundColor: '#fff',
    padding: 12,
    margin: 12,
    borderRadius: 12,
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 3,
    elevation: 2,
  },
  stat: { flex: 1, alignItems: 'center' },
  statValue: { fontSize: 22, fontWeight: 'bold', color: '#333' },
  statLabel: { fontSize: 11, color: '#999', marginTop: 2 },
  syncButton: {
    backgroundColor: '#007AFF',
    paddingVertical: 8,
    paddingHorizontal: 16,
    borderRadius: 8,
  },
  syncButtonText: { color: '#fff', fontSize: 13, fontWeight: '600' },
  listContainer: { padding: 12, paddingTop: 0, flexGrow: 1 },
  card: {
    flexDirection: 'row',
    backgroundColor: '#fff',
    padding: 12,
    borderRadius: 12,
    alignItems: 'center',
    marginBottom: 10,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 3,
    elevation: 2,
  },
  avatar: {
    width: 44,
    height: 44,
    borderRadius: 22,
    backgroundColor: '#007AFF',
    justifyContent: 'center',
    alignItems: 'center',
  },
  avatarText: { color: '#fff', fontSize: 18, fontWeight: 'bold' },
  info: { flex: 1, marginLeft: 12 },
  nameText: { fontSize: 15, fontWeight: 'bold', color: '#333' },
  timeText: { fontSize: 12, color: '#666', marginTop: 2 },
  rightCol: { alignItems: 'flex-end' },
  statusPill: { paddingHorizontal: 8, paddingVertical: 3, borderRadius: 12, marginBottom: 4 },
  syncedPill: { backgroundColor: '#E8F5E9' },
  unsyncedPill: { backgroundColor: '#FFF3E0' },
  statusText: { fontSize: 11, fontWeight: '600' },
  syncedText: { color: '#2E7D32' },
  unsyncedText: { color: '#E65100' },
  confidenceText: { fontSize: 12, color: '#999' },
  emptyContainer: { flex: 1, justifyContent: 'center', alignItems: 'center', marginTop: 80 },
  emptyTitle: { fontSize: 18, fontWeight: 'bold', color: '#999' },
  emptySubtitle: { fontSize: 14, color: '#bbb', marginTop: 6 },
});
