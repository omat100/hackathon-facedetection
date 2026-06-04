import { Tabs } from 'expo-router';
import React from 'react';
import { Platform } from 'react-native';
// Use the robust Expo Font-Awesome vector pack directly
import FontAwesome from '@expo/vector-icons/FontAwesome';

export default function TabLayout() {
  return (
    <Tabs
      screenOptions={{
        tabBarActiveTintColor: '#007AFF', // Clean iOS blue tint
        headerShown: true, 
        tabBarStyle: Platform.select({
          ios: { position: 'absolute' }, 
          default: {},
        }),
      }}>
      <Tabs.Screen
        name="index"
        options={{
          title: 'Home',
          tabBarIcon: ({ color }) => <FontAwesome size={24} name="home" color={color} />,
        }}
      />
      <Tabs.Screen
        name="explore" 
        options={{
          title: 'Mark Attendance',
          tabBarIcon: ({ color }) => <FontAwesome size={24} name="camera" color={color} />,
        }}
      />
    </Tabs>
  );
}