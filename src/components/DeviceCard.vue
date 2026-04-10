<script setup>
import { ref } from 'vue'

defineProps({
  deviceId: {
    type: String,
    required: true,
  },
  device: {
    type: Object,
    required: true,
  },
  timer: {
    type: Object,
    default: null,
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

const emit = defineEmits(['toggle', 'brightness-change', 'set-timer', 'cancel-timer'])

const showTimerConfig = ref(false)
const timerMinutes = ref(5)

function formatUpdatedAt(timestamp) {
  if (!timestamp) {
    return 'Bez zmeny'
  }

  return new Intl.DateTimeFormat('sk-SK', {
    dateStyle: 'short',
    timeStyle: 'short',
  }).format(new Date(timestamp))
}

function formatTimeRemaining(remainingMs) {
  if (remainingMs <= 0) return '0'
  const minutes = Math.floor(remainingMs / 60000)
  const seconds = Math.floor((remainingMs % 60000) / 1000)
  return `${minutes}:${String(seconds).padStart(2, '0')}`
}

function handleSetTimer() {
  const minutes = Number(timerMinutes.value)
  if (minutes <= 0 || minutes > 1440) {
    return
  }

  emit('set-timer', {
    action: 'toggle',
    durationMs: minutes * 60000,
  })
  showTimerConfig.value = false
}

function handleCancelTimer() {
  emit('cancel-timer')
  showTimerConfig.value = false
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
    <details class="timer-section">
      <summary class="timer-summary">
        <span>⏲️ Prepnúť stav</span>
        <span v-if="timer" class="timer-badge">
          {{ formatTimeRemaining(timer.remainingMs) }}
        </span>
      </summary>

      <div class="timer-content">
        <div v-if="timer" class="timer-active">
          <p class="timer-info">
            <strong>Časovač aktívny:</strong> {{ timer.actionLabel }}
            za {{ formatTimeRemaining(timer.remainingMs) }}
          </p>
          <button class="danger-btn" type="button" @click="handleCancelTimer">
            Zrušiť časovač
          </button>
        </div>

        <div v-else class="timer-form">
          <label for="timer-minutes-{{ deviceId }}">
            <span>Čas (minúty):</span>
            <input
              :id="`timer-minutes-${deviceId}`"
              v-model.number="timerMinutes"
              type="number"
              min="1"
              max="1440"
              :disabled="busy || disabled"
            />
          </label>



          <button
            class="primary-btn"
            type="button"
            :disabled="busy || disabled || timerMinutes <= 0"
            @click="handleSetTimer"
          >
            Nastaviť časovač
          </button>
        </div>
      </div>
    </details>  </article>
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

.timer-section {
  border-top: 1px solid #e0f0f5;
  padding-top: 1rem;
}

.timer-summary {
  display: flex;
  justify-content: space-between;
  align-items: center;
  cursor: pointer;
  color: #1f7a8c;
  font-weight: 600;
  font-size: 0.95rem;
  padding: 0.5rem 0;
  user-select: none;
}

.timer-summary:hover {
  opacity: 0.8;
}

.timer-badge {
  background: #1f7a8c;
  color: white;
  border-radius: 999px;
  padding: 0.25rem 0.65rem;
  font-size: 0.85rem;
  font-weight: 700;
}

.timer-content {
  padding-top: 0.75rem;
}

.timer-active {
  display: grid;
  gap: 0.75rem;
  background: #f0f8fa;
  border: 1px solid #d5e8ed;
  border-radius: 10px;
  padding: 0.75rem;
}

.timer-info {
  margin: 0;
  color: #1f7a8c;
  font-size: 0.95rem;
}

.timer-form {
  display: grid;
  gap: 0.75rem;
}

.timer-form label {
  display: grid;
  gap: 0.35rem;
  color: #365268;
  font-size: 0.9rem;
}

.timer-form span {
  font-weight: 600;
}

.timer-form input[type='number'],
.timer-form select {
  border: 1px solid #c5dce4;
  border-radius: 8px;
  padding: 0.5rem;
  font-size: 0.95rem;
  color: #365268;
}

.timer-form input[type='number']:disabled,
.timer-form select:disabled {
  background: #f5f5f5;
  cursor: not-allowed;
}

.brightness-input-row {
  display: flex;
  align-items: center;
  gap: 0.75rem;
}

.brightness-input-row input {
  flex: 1;
  accent-color: #1f7a8c;
}

.brightness-input-row strong {
  min-width: 3rem;
  text-align: right;
  font-size: 0.9rem;
}

.primary-btn {
  border: none;
  border-radius: 8px;
  padding: 0.6rem 1rem;
  font-weight: 600;
  cursor: pointer;
  color: white;
  background: linear-gradient(120deg, #1f7a8c, #1d4f77);
  font-size: 0.95rem;
  transition: opacity 0.2s;
}

.primary-btn:disabled {
  cursor: not-allowed;
  opacity: 0.5;
}

.primary-btn:not(:disabled):hover {
  opacity: 0.9;
}

.danger-btn {
  border: none;
  border-radius: 8px;
  padding: 0.6rem 1rem;
  font-weight: 600;
  cursor: pointer;
  color: white;
  background: #c85a5a;
  font-size: 0.95rem;
  transition: opacity 0.2s;
}
</style>