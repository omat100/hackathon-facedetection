import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ActivityIndicator,
  Dimensions,
} from 'react-native';
import { AppState } from '../types';

const { width, height } = Dimensions.get('window');

interface LivenessPromptProps {
  state: AppState;
  progress: number;
}

export const LivenessPrompt: React.FC<LivenessPromptProps> = ({
  state,
  progress,
}) => {
  const getPromptText = (): string => {
    switch (state) {
      case AppState.LIVENESS_CHECK:
        if (progress < 25) return 'Look straight at the camera';
        if (progress < 50) return 'Slowly turn your head to the left';
        if (progress < 75) return 'Slowly turn your head to the right';
        return 'Smile for the camera';
      case AppState.FACE_DETECTION:
        return 'Detecting face...';
      case AppState.SUCCESS:
        return 'Attendance marked!';
      case AppState.FAILED:
        return 'Detection failed, try again';
      default:
        return '';
    }
  };

  const getPromptColor = (): string => {
    switch (state) {
      case AppState.SUCCESS:
        return '#4CAF50';
      case AppState.FAILED:
        return '#F44336';
      case AppState.LIVENESS_CHECK:
      case AppState.FACE_DETECTION:
        return '#2196F3';
      default:
        return '#666';
    }
  };

  return (
    <View style={[styles.container, { backgroundColor: getPromptColor() }]}>
      <Text style={styles.promptText}>{getPromptText()}</Text>
      {state !== AppState.SUCCESS && state !== AppState.FAILED && (
        <>
          <ActivityIndicator
            size="large"
            color="#fff"
            style={styles.spinner}
          />
          <View style={styles.progressBar}>
            <View
              style={[
                styles.progressFill,
                { width: `${progress}%` },
              ]}
            />
          </View>
          <Text style={styles.progressText}>{Math.round(progress)}%</Text>
        </>
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    height: height * 0.2,
    justifyContent: 'center',
    alignItems: 'center',
    paddingHorizontal: 20,
    zIndex: 10,
  },
  promptText: {
    fontSize: 20,
    fontWeight: 'bold',
    color: '#fff',
    textAlign: 'center',
    marginBottom: 16,
  },
  spinner: {
    marginVertical: 12,
  },
  progressBar: {
    width: '80%',
    height: 6,
    backgroundColor: 'rgba(255,255,255,0.3)',
    borderRadius: 3,
    overflow: 'hidden',
    marginTop: 12,
  },
  progressFill: {
    height: '100%',
    backgroundColor: '#fff',
  },
  progressText: {
    color: '#fff',
    fontSize: 12,
    marginTop: 8,
    fontWeight: '600',
  },
});
