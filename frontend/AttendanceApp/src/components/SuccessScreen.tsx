import React from 'react';
import {
  View,
  Text,
  StyleSheet,
  TouchableOpacity,
  Dimensions,
} from 'react-native';

const { width, height } = Dimensions.get('window');

interface SuccessScreenProps {
  personName: string;
  confidence: number;
  onDismiss: () => void;
  timestamp: string;
}

export const SuccessScreen: React.FC<SuccessScreenProps> = ({
  personName,
  confidence,
  onDismiss,
  timestamp,
}) => {
  return (
    <View style={styles.container}>
      <View style={styles.content}>
        <Text style={styles.checkmark}>✓</Text>
        <Text style={styles.title}>Attendance Marked!</Text>
        <Text style={styles.personName}>{personName}</Text>

        <View style={styles.detailsContainer}>
          <Text style={styles.detailLabel}>Confidence:</Text>
          <Text style={styles.detailValue}>
            {(confidence * 100).toFixed(1)}%
          </Text>

          <Text style={styles.detailLabel}>Time:</Text>
          <Text style={styles.detailValue}>
            {new Date(timestamp).toLocaleTimeString()}
          </Text>
        </View>

        <TouchableOpacity
          style={styles.button}
          onPress={onDismiss}
          activeOpacity={0.7}
        >
          <Text style={styles.buttonText}>Mark Another</Text>
        </TouchableOpacity>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    ...StyleSheet.absoluteFillObject,
    backgroundColor: 'rgba(0, 0, 0, 0.9)',
    justifyContent: 'center',
    alignItems: 'center',
    zIndex: 100,
  },
  content: {
    backgroundColor: '#fff',
    borderRadius: 20,
    padding: 30,
    alignItems: 'center',
    width: width * 0.85,
  },
  checkmark: {
    fontSize: 80,
    color: '#4CAF50',
    marginBottom: 20,
  },
  title: {
    fontSize: 28,
    fontWeight: 'bold',
    color: '#333',
    marginBottom: 12,
  },
  personName: {
    fontSize: 22,
    fontWeight: '600',
    color: '#4CAF50',
    marginBottom: 20,
  },
  detailsContainer: {
    width: '100%',
    backgroundColor: '#f5f5f5',
    borderRadius: 12,
    padding: 16,
    marginBottom: 24,
  },
  detailLabel: {
    fontSize: 12,
    color: '#999',
    fontWeight: '600',
    marginTop: 8,
  },
  detailValue: {
    fontSize: 14,
    color: '#333',
    fontWeight: '500',
    marginBottom: 8,
  },
  button: {
    backgroundColor: '#4CAF50',
    paddingVertical: 12,
    paddingHorizontal: 32,
    borderRadius: 8,
    width: '100%',
  },
  buttonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '600',
    textAlign: 'center',
  },
});
