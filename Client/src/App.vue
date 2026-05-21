<template>
  <router-view />
</template>

<script setup>
import { useAppStore } from '@/stores/app'
// Initialize the app store to apply theme on startup
useAppStore()
</script>
