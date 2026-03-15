<script setup>
defineProps({
  device: {
    type: Object,
    required: true,
  },
  busy: {
    type: Boolean,
    default: false,
  },
})

defineEmits(['toggle'])
</script>

<template>
  <article class="device-card">
    <header class="device-head">
      <h3>{{ device.name }}</h3>
      <span class="status" :class="device.isOn ? 'on' : 'off'">
        {{ device.isOn ? 'Zapnuty' : 'Vypnuty' }}
      </span>
    </header>

    <p class="metric">Aktualny vykon: <strong>{{ Number(device.powerW || 0).toFixed(2) }} W</strong></p>

    <button class="toggle-btn" :disabled="busy" @click="$emit('toggle')">
      {{ device.isOn ? 'Vypnut' : 'Zapnut' }}
    </button>
  </article>
</template>

<style scoped>
.device-card {
  background: #ffffff;
  border: 1px solid #d5e3e8;
  border-radius: 18px;
  padding: 1.2rem;
  box-shadow: 0 12px 24px rgba(10, 35, 56, 0.08);
}

.device-head {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 0.8rem;
}

h3 {
  margin: 0;
  font-size: 1.1rem;
}

.status {
  border-radius: 999px;
  padding: 0.3rem 0.8rem;
  font-size: 0.85rem;
  font-weight: 600;
}

.status.on {
  background: #dbf5e6;
  color: #107243;
}

.status.off {
  background: #f9e0de;
  color: #8f2525;
}

.metric {
  margin: 1rem 0;
  color: #365268;
}

.toggle-btn {
  width: 100%;
  border: none;
  border-radius: 12px;
  padding: 0.7rem 1rem;
  font-weight: 700;
  cursor: pointer;
  color: #ffffff;
  background: linear-gradient(120deg, #1f7a8c, #1d4f77);
}

.toggle-btn:disabled {
  cursor: not-allowed;
  opacity: 0.6;
}
</style>
