import React from 'react';
import {
  View,
  Text,
  FlatList,
  StyleSheet,
  ActivityIndicator,
  RefreshControl,
} from 'react-native';
import { AttendanceRecord } from '../types';

interface AttendanceListProps {
  records: AttendanceRecord[];
  loading: boolean;
  onRefresh: () => void;
}

interface AttendanceItemProps {
  record: AttendanceRecord;
}

const AttendanceItem: React.FC<AttendanceItemProps> = ({ record }) => {
  const date = new Date(record.timestamp);
  const timeString = date.toLocaleTimeString('en-US', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });

  return (
    <View style={styles.item}>
      <View style={styles.itemContent}>
        <Text style={styles.personId}>{record.person_id}</Text>
        <Text style={styles.timestamp}>{timeString}</Text>
      </View>
      <View style={styles.rightContent}>
        <View
          style={[
            styles.confidenceBadge,
            {
              backgroundColor:
                record.confidence > 0.85
                  ? '#4CAF50'
                  : record.confidence > 0.7
                    ? '#FFC107'
                    : '#FF9800',
            },
          ]}
        >
          <Text style={styles.confidenceText}>
            {(record.confidence * 100).toFixed(0)}%
          </Text>
        </View>
        <View
          style={[
            styles.syncBadge,
            {
              backgroundColor: record.synced ? '#4CAF50' : '#9E9E9E',
            },
          ]}
        >
          <Text style={styles.syncText}>
            {record.synced ? '✓ Synced' : '○ Local'}
          </Text>
        </View>
      </View>
    </View>
  );
};

export const AttendanceList: React.FC<AttendanceListProps> = ({
  records,
  loading,
  onRefresh,
}) => {
  if (loading && records.length === 0) {
    return (
      <View style={styles.loadingContainer}>
        <ActivityIndicator size="large" color="#2196F3" />
        <Text style={styles.loadingText}>Loading attendance records...</Text>
      </View>
    );
  }

  if (records.length === 0) {
    return (
      <View style={styles.emptyContainer}>
        <Text style={styles.emptyIcon}>📋</Text>
        <Text style={styles.emptyText}>No attendance records yet</Text>
        <Text style={styles.emptySubtext}>
          Mark attendance by opening the camera
        </Text>
      </View>
    );
  }

  return (
    <FlatList
      data={records}
      keyExtractor={(item) => item.id?.toString() || Math.random().toString()}
      renderItem={({ item }) => <AttendanceItem record={item} />}
      scrollEnabled={true}
      contentContainerStyle={styles.listContent}
      refreshControl={
        <RefreshControl
          refreshing={loading}
          onRefresh={onRefresh}
          colors={['#2196F3']}
        />
      }
      style={styles.list}
    />
  );
};

const styles = StyleSheet.create({
  list: {
    flex: 1,
  },
  listContent: {
    paddingHorizontal: 12,
    paddingVertical: 8,
  },
  loadingContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingVertical: 40,
  },
  loadingText: {
    marginTop: 16,
    color: '#666',
    fontSize: 14,
  },
  emptyContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingVertical: 60,
  },
  emptyIcon: {
    fontSize: 64,
    marginBottom: 16,
  },
  emptyText: {
    fontSize: 18,
    fontWeight: '600',
    color: '#333',
    marginBottom: 8,
  },
  emptySubtext: {
    fontSize: 14,
    color: '#999',
  },
  item: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    backgroundColor: '#fff',
    borderRadius: 12,
    padding: 16,
    marginVertical: 6,
    elevation: 2,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 3,
  },
  itemContent: {
    flex: 1,
  },
  personId: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
    marginBottom: 4,
  },
  timestamp: {
    fontSize: 12,
    color: '#999',
  },
  rightContent: {
    gap: 8,
    alignItems: 'flex-end',
  },
  confidenceBadge: {
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: 6,
  },
  confidenceText: {
    color: '#fff',
    fontSize: 12,
    fontWeight: '600',
  },
  syncBadge: {
    paddingHorizontal: 10,
    paddingVertical: 4,
    borderRadius: 6,
  },
  syncText: {
    color: '#fff',
    fontSize: 10,
    fontWeight: '600',
  },
});
