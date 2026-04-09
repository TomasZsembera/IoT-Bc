<script setup>
defineProps({
  deviceId: {
    type: String,
    required: true,
  },
  device: {
    type: Object,
    required: true,
  },
  busy: {
    type: Boolean,
    default: false,
  },
  disabled: {
    type: Boolean,
    default: false,
  },
})

defineEmits(['toggle', 'brightness-change'])

function formatUpdatedAt(timestamp) {
  if (!timestamp) {
    return 'Bez zmeny'
  }

  return new Intl.DateTimeFormat('sk-SK', {
    dateStyle: 'short',
    timeStyle: 'short',
  }).format(new Date(timestamp))
}
</script>

<template>
  <article class="device-card" :class="{ active: device.isOn && !disabled, disabled }">
    <header class="device-head">
      <div>
        <h3>{{ device.name }}</h3>
      </div>
      <span class="status" :class="device.isOn ? 'on' : 'off'">
        {{ device.isOn ? 'Zapnuty' : 'Vypnuty' }}
      </span>
    </header>

    <div class="metric-stack">
      <p class="metric">Aktuálny výkon: <strong>{{ Number(device.powerW || 0).toFixed(2) }} W</strong></p>
      <p v-if="device.supportsBrightness" class="metric">Jas: <strong>{{ Number(device.brightness || 0).toFixed(0) }}%</strong></p>
      <p class="updated-at">Posledná zmena: {{ formatUpdatedAt(device.updatedAt) }}</p>
    </div>

    <label v-if="device.supportsBrightness" class="brightness-control" :for="`brightness-${deviceId}`">
      <span>Jas LED</span>
      <div class="brightness-row">
        <input
          :id="`brightness-${deviceId}`"
          :disabled="busy || disabled"
          :value="device.brightness"
          class="brightness-slider"
          max="100"
          min="0"
          type="range"
          @input="!disabled && $emit('brightness-change', Number($event.target.value))"
        />
        <strong>{{ Number(device.brightness || 0).toFixed(0) }}%</strong>
      </div>
    </label>

    <button class="toggle-btn" :disabled="busy || disabled" @click="!disabled && $emit('toggle')">
      {{ device.isOn ? 'Vypnúť' : 'Zapnúť' }}
    </button>
  </article>
</template>

<style scoped>
.device-card {
  display: grid;
  gap: 1rem;
  background: linear-gradient(180deg, rgba(255, 255, 255, 0.98), rgba(243, 248, 250, 0.98));
  border: 1px solid #d5e3e8;
  border-radius: 18px;
  padding: 1.2rem;
  box-shadow: 0 12px 24px rgba(10, 35, 56, 0.08);
  transition: transform 0.2s ease, box-shadow 0.2s ease, border-color 0.2s ease;
}

.device-card.active {
  border-color: rgba(31, 122, 140, 0.45);
  box-shadow: 0 18px 34px rgba(10, 35, 56, 0.14);
  transform: translateY(-1px);
}

.device-card.disabled {
  opacity: 0.55;
  pointer-events: none;
}

.device-head {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 0.8rem;
}

h3 {
  margin: 0;
  font-size: 1.15rem;
}

.device-kicker {
  margin: 0 0 0.2rem;
  color: #4a6572;
  font-size: 0.78rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
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

.metric-stack {
  display: grid;
  gap: 0.4rem;
}

.metric {
  margin: 0;
  color: #365268;
}

.updated-at {
  margin: 0;
  color: #5f7888;
  font-size: 0.9rem;
}

.brightness-control {
  display: grid;
  gap: 0.55rem;
  color: #365268;
  font-size: 0.95rem;
}

.brightness-row {
  display: flex;
  align-items: center;
  gap: 0.75rem;
}

.brightness-row strong {
  min-width: 3rem;
  text-align: right;
}

.brightness-slider {
  flex: 1;
  accent-color: #1f7a8c;
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