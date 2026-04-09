<script setup>
import { computed, onMounted, onUnmounted, reactive, ref } from 'vue'
import {
  onAuthStateChanged,
  signInWithEmailAndPassword,
  signOut,
} from 'firebase/auth'
import { onValue, ref as dbRef, update, remove } from 'firebase/database'
import DeviceCard from './components/DeviceCard.vue'
import ConsumptionChart from './components/ConsumptionChart.vue'
import { auth, db, isFirebaseConfigured, useMockData } from './firebase'

const DEVICE_IDS = ['device1', 'device2']
const DEVICE_CONFIG = {
  device1: {
    name: 'LED',
    supportsBrightness: true,
  },
  device2: {
    name: 'Fan',
    supportsBrightness: false,
  },
}

const AUTH_ERROR_MESSAGES = {
  'auth/email-already-in-use': 'Tento email je uz registrovany. Skus sa prihlasit.',
  'auth/invalid-credential': 'Zle prihlasovacie udaje. Skontroluj email a heslo.',
  'auth/invalid-email': 'Email nema spravny format.',
  'auth/operation-not-allowed': 'Prihlasovanie cez email/heslo nie je v Firebase zapnute.',
  'auth/weak-password': 'Heslo je prilis kratke. Zadaj aspon 6 znakov.',
  'auth/user-disabled': 'Tento ucet je deaktivovany.',
}

const authLoading = ref(true)
const authReady = ref(false)
const user = ref(null)
const email = ref('')
const password = ref('')
const authError = ref('')
const authBusy = ref(false)
const actionBusy = ref(false)
const clockNow = ref(Date.now())

const devices = reactive({
  device1: {
    name: DEVICE_CONFIG.device1.name,
    isOn: false,
    powerW: 0,
    brightness: 0,
    supportsBrightness: DEVICE_CONFIG.device1.supportsBrightness,
    updatedAt: 0,
  },
  device2: {
    name: DEVICE_CONFIG.device2.name,
    isOn: false,
    powerW: 0,
    brightness: 0,
    supportsBrightness: DEVICE_CONFIG.device2.supportsBrightness,
    updatedAt: 0,
  },
})

const monthlyData = reactive({
  device1: { labels: [], values: [] },
  device2: { labels: [], values: [] },
})

const telemetry = reactive({
  device1: { lastUpdatedAt: 0, totalKwh30d: 0 },
  device2: { lastUpdatedAt: 0, totalKwh30d: 0 },
})

let authUnsubscribe = null
const dbUnsubscribers = []
let mockInterval = null
let clockInterval = null
let timerCheckInterval = null

const isLoggedIn = computed(() => !!user.value)
const signedInEmail = computed(() => user.value?.email || 'Neznáme konto')
const latestTelemetryTimestamp = computed(() => {
  return Math.max(...DEVICE_IDS.map((deviceId) => devices[deviceId].updatedAt || 0), 0)
})
const telemetryAgeMinutes = computed(() => {
  if (!latestTelemetryTimestamp.value) {
    return null
  }

  return Math.round((clockNow.value - latestTelemetryTimestamp.value) / 60000)
})
const telemetryHealthy = computed(() => {
  return latestTelemetryTimestamp.value && (telemetryAgeMinutes.value || 0) <= 20
})
const telemetryHealthyLabel = computed(() => {
  if (!latestTelemetryTimestamp.value) {
    return 'Čakám na prvý update'
  }

  if (telemetryHealthy.value) {
    return 'Online'
  }

  return 'Offline'
})
const telemetryHealthyClass = computed(() => {
  if (!latestTelemetryTimestamp.value) {
    return 'status-neutral'
  }

  return telemetryHealthy.value ? 'status-ok' : 'status-warn'
})
const latestTelemetryLabel = computed(() => {
  if (!latestTelemetryTimestamp.value) {
    return 'Zatial nič'
  }

  return new Intl.DateTimeFormat('sk-SK', {
    dateStyle: 'short',
    timeStyle: 'short',
  }).format(new Date(latestTelemetryTimestamp.value))
})

function normalizePowerW(value) {
  const numeric = Number(value)
  return Number.isFinite(numeric) ? Number(numeric.toFixed(2)) : 0
}

function normalizeBrightness(value) {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) {
    return 0
  }

  return Math.min(100, Math.max(0, Math.round(numeric)))
}

function getDefaultDeviceState(deviceId, value = {}) {
  const config = DEVICE_CONFIG[deviceId]
  const scheduledAction = value.scheduledAction || null

  let timerInfo = null
  if (scheduledAction?.executeAtMs) {
    const remaining = scheduledAction.executeAtMs - Date.now()
    if (remaining > 0) {
      timerInfo = {
        action: scheduledAction.action,
        brightness: scheduledAction.brightness || null,
        executeAtMs: scheduledAction.executeAtMs,
        remainingMs: remaining,
        actionLabel: getActionLabel(scheduledAction.action, { isOn: value.isOn }, scheduledAction.brightness),
      }
    }
  }

  return {
    name: value.name || config.name,
    isOn: Boolean(value.isOn),
    powerW: normalizePowerW(value.powerW),
    brightness: config.supportsBrightness ? normalizeBrightness(value.brightness) : 0,
    supportsBrightness: config.supportsBrightness,
    updatedAt: Number(value.updatedAt) || 0,
    scheduledAction: timerInfo,
  }
}

function updateDeviceState(deviceId, patch) {
  devices[deviceId] = {
    ...devices[deviceId],
    ...patch,
  }
}

function formatDateKey(timestamp) {
  const date = new Date(timestamp)
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`
}

function labelFromDateKey(dateKey) {
  const [year, month, day] = dateKey.split('-')
  return `${day}.${month}.${year}`
}

function clearRealtimeListeners() {
  while (dbUnsubscribers.length > 0) {
    const unsubscribe = dbUnsubscribers.pop()
    if (typeof unsubscribe === 'function') {
      unsubscribe()
    }
  }
}

function loadMockMonthlyData() {
  const now = Date.now()

  DEVICE_IDS.forEach((deviceId, deviceIndex) => {
    const labels = []
    const values = []

    for (let i = 29; i >= 0; i -= 1) {
      const timestamp = now - i * 24 * 60 * 60 * 1000
      labels.push(labelFromDateKey(formatDateKey(timestamp)))

      const base = deviceIndex === 0 ? 0.95 : 1.55
      const wave = Math.sin((29 - i) / 4) * 0.25
      const jitter = Math.random() * 0.18
      values.push(Number((base + wave + jitter).toFixed(3)))
    }

    monthlyData[deviceId] = { labels, values }
  })
}

function applyMockDeviceValues() {
  DEVICE_IDS.forEach((deviceId, deviceIndex) => {
    const current = devices[deviceId]
    const basePower = deviceIndex === 0 ? 140 : 220
    const fluctuation = Math.random() * 24
    const brightnessMultiplier = current.supportsBrightness ? Math.max(current.brightness, current.isOn ? 55 : 0) / 100 : 1

    devices[deviceId] = {
      ...current,
      powerW: current.isOn ? Number((basePower * brightnessMultiplier + fluctuation).toFixed(2)) : 0,
      updatedAt: Date.now(),
    }
  })
}

function startMockMode() {
  user.value = {
    uid: 'demo-user',
    email: 'demo@iot.local',
  }
  authReady.value = true
  authLoading.value = false
  loadMockMonthlyData()
  applyMockDeviceValues()

  mockInterval = setInterval(() => {
    applyMockDeviceValues()
  }, 3000)
}

function formatAuthError(error) {
  return AUTH_ERROR_MESSAGES[error?.code] || 'Prihlasenie zlyhalo. Skontroluj Firebase Auth a pokus sa znova.'
}

function getConsumptionValue(entry) {
  if (!entry || typeof entry !== 'object') {
    return 0
  }

  const fromKwh = Number(entry.kWh)
  if (Number.isFinite(fromKwh)) {
    return fromKwh
  }

  const fromWh = Number(entry.energyWh)
  if (Number.isFinite(fromWh)) {
    return fromWh / 1000
  }

  return 0
}

function buildMonthlySeries(rawEntries) {
  const now = Date.now()
  const days = 30
  const totalsByDay = new Map()
  let totalKwh30d = 0

  Object.values(rawEntries || {}).forEach((entry) => {
    const timestamp = Number(entry?.timestamp)
    if (!Number.isFinite(timestamp)) {
      return
    }

    const kwh = getConsumptionValue(entry)
    if (kwh <= 0) {
      return
    }

    const dateKey = formatDateKey(timestamp)
    totalsByDay.set(dateKey, (totalsByDay.get(dateKey) || 0) + kwh)
  })

  const labels = []
  const values = []

  for (let i = days - 1; i >= 0; i -= 1) {
    const timestamp = now - i * 24 * 60 * 60 * 1000
    const key = formatDateKey(timestamp)
    const dayValue = Number((totalsByDay.get(key) || 0).toFixed(3))

    labels.push(labelFromDateKey(key))
    values.push(dayValue)
    totalKwh30d += dayValue
  }

  return {
    labels,
    values,
    totalKwh30d: Number(totalKwh30d.toFixed(3)),
  }
}

function listenToConsumptionLogs() {
  DEVICE_IDS.forEach((deviceId) => {
    const unsubscribe = onValue(dbRef(db, `consumptionLogs/${deviceId}`), (snapshot) => {
      const { labels, values, totalKwh30d } = buildMonthlySeries(snapshot.val() || {})

      monthlyData[deviceId] = { labels, values }
      telemetry[deviceId].totalKwh30d = totalKwh30d
    })

    dbUnsubscribers.push(unsubscribe)
  })
}

function listenToDevices() {
  clearRealtimeListeners()

  DEVICE_IDS.forEach((deviceId) => {
    const unsubscribe = onValue(dbRef(db, `devices/${deviceId}`), (snapshot) => {
      const value = snapshot.val() ?? {}
      devices[deviceId] = getDefaultDeviceState(deviceId, value)
    })

    dbUnsubscribers.push(unsubscribe)
  })

  listenToConsumptionLogs()
}

async function handleAuthSubmit() {
  authError.value = ''
  authBusy.value = true

  try {
    await signInWithEmailAndPassword(auth, email.value, password.value)
    password.value = ''
  } catch (error) {
    authError.value = formatAuthError(error)
    console.error(error)
  } finally {
    authBusy.value = false
  }
}

async function handleLogout() {
  if (useMockData) {
    return
  }
  await signOut(auth)
}

async function toggleDevice(deviceId) {
  if (actionBusy.value) {
    return
  }

  if (useMockData) {
    const nextState = !devices[deviceId].isOn
    const nextBrightness = nextState && devices[deviceId].supportsBrightness && devices[deviceId].brightness === 0 ? 60 : devices[deviceId].brightness

    updateDeviceState(deviceId, {
      isOn: nextState,
      brightness: nextBrightness,
      updatedAt: Date.now(),
    })
    applyMockDeviceValues()
    return
  }

  actionBusy.value = true
  try {
    const next = !devices[deviceId].isOn
    const payload = {
      isOn: next,
      updatedAt: Date.now(),
    }

    if (next && devices[deviceId].supportsBrightness && devices[deviceId].brightness === 0) {
      payload.brightness = 60
    }

    await update(dbRef(db, `devices/${deviceId}`), payload)
  } finally {
    actionBusy.value = false
  }
}

async function updateBrightness(deviceId, brightness) {
  if (actionBusy.value) {
    return
  }

  const nextBrightness = normalizeBrightness(brightness)

  if (useMockData) {
    updateDeviceState(deviceId, {
      brightness: nextBrightness,
      isOn: nextBrightness > 0,
      updatedAt: Date.now(),
    })
    applyMockDeviceValues()
    return
  }

  actionBusy.value = true
  try {
    await update(dbRef(db, `devices/${deviceId}`), {
      brightness: nextBrightness,
      isOn: nextBrightness > 0,
      updatedAt: Date.now(),
    })
  } finally {
    actionBusy.value = false
  }
}

function getActionLabel(action, device, brightness = null) {
  switch (action) {
    case 'toggle':
      return device.isOn ? 'Vypnúť' : 'Zapnúť'
    case 'on':
      return 'Zapnúť'
    case 'off':
      return 'Vypnúť'
    case 'brightness':
      return `Jas: ${brightness}%`
    default:
      return 'Akcia'
  }
}

async function executeTimerAction(deviceId, action, brightness = null) {
  const device = devices[deviceId]

  switch (action) {
    case 'toggle':
      await toggleDevice(deviceId)
      break
    case 'on':
      if (!device.isOn) {
        await toggleDevice(deviceId)
      }
      break
    case 'off':
      if (device.isOn) {
        await toggleDevice(deviceId)
      }
      break
    case 'brightness':
      if (brightness !== null) {
        await updateBrightness(deviceId, brightness)
      }
      break
    default:
      console.warn('Unknown timer action:', action)
  }
}

function setDeviceTimer(deviceId, action, durationMs, brightness = null) {
  const executeAtMs = Date.now() + durationMs

  const payload = {
    scheduledAction: {
      action,
      executeAtMs,
    },
  }

  if (brightness !== null) {
    payload.scheduledAction.brightness = brightness
  }

  return update(dbRef(db, `devices/${deviceId}`), payload)
}

function cancelDeviceTimer(deviceId) {
  return remove(dbRef(db, `devices/${deviceId}/scheduledAction`))
}

async function checkAndExecuteScheduledActions() {
  const now = Date.now()

  for (const deviceId of DEVICE_IDS) {
    const device = devices[deviceId]
    if (!device.scheduledAction) {
      continue
    }

    const { executeAtMs, action, brightness } = device.scheduledAction

    if (executeAtMs <= now) {
      // Čas spustiť akciu
      await executeTimerAction(deviceId, action, brightness)

      // Zmaž scheduledAction z Firebase
      await remove(dbRef(db, `devices/${deviceId}/scheduledAction`))
    } else {
      // Aktualizuj remaining čas
      device.scheduledAction.remainingMs = executeAtMs - now
    }
  }
}

onMounted(() => {
  clockInterval = setInterval(() => {
    clockNow.value = Date.now()
  }, 60000)

  timerCheckInterval = setInterval(() => {
    checkAndExecuteScheduledActions()
  }, 500)

  if (useMockData) {
    startMockMode()
    return
  }

  if (!isFirebaseConfigured) {
    authLoading.value = false
    authReady.value = false
    return
  }

  authUnsubscribe = onAuthStateChanged(auth, async (firebaseUser) => {
    user.value = firebaseUser
    authReady.value = true

    if (firebaseUser) {
      listenToDevices()
    } else {
      clearRealtimeListeners()
    }

    authLoading.value = false
  })
})

onUnmounted(() => {
  if (mockInterval) {
    clearInterval(mockInterval)
  }

  if (clockInterval) {
    clearInterval(clockInterval)
  }

  if (timerCheckInterval) {
    clearInterval(timerCheckInterval)
  }

  clearRealtimeListeners()
  if (typeof authUnsubscribe === 'function') {
    authUnsubscribe()
  }
})
</script>

<template>
  <div class="page-shell">
    <header class="topbar">
      <div class="brand-block">
        <p class="eyebrow">IoT Energetický Panel</p>
        <h1>Ovládanie & Meranie spotreby</h1>
      </div>

      <div v-if="isLoggedIn" class="session-chip">
        <div>
          <span class="session-label">Prihlásený účet</span>
          <strong>{{ signedInEmail }}</strong>
        </div>
        <button class="secondary-btn" :disabled="authBusy" @click="handleLogout">Odhlásiť sa</button>
      </div>
    </header>

    <section v-if="!isFirebaseConfigured && !useMockData" class="message-card warning">
      <h2>Firebase konfiguracia chyba</h2>
      <p>
        Vytvor subor <strong>.env</strong> a dopln Firebase hodnoty z konzoly.
      </p>
    </section>

    <section v-else-if="authLoading" class="message-card">
      <p>Nacitavam autentifikaciu...</p>
    </section>

    <section v-else-if="!isLoggedIn && authReady" class="auth-card">
      <p class="eyebrow">Bezpečný prístup</p>
      <h2>Prihlásenie</h2>
      <p>Prihlásenie dostali zatial len vybraný užívatelia.</p>

      <form class="auth-form" @submit.prevent="handleAuthSubmit">
        <label>
          Email
          <input v-model.trim="email" type="email" required autocomplete="email" placeholder="meno@domena.sk" />
        </label>

        <label>
          Heslo
          <input v-model="password" type="password" required minlength="6" autocomplete="current-password" placeholder="Tvoje heslo" />
        </label>

        <button class="primary-btn" :disabled="authBusy" type="submit">
          {{ authBusy ? 'Spracuvávam...' : 'Prihlasit sa' }}
        </button>
      </form>

      <p v-if="authError" class="error-text">{{ authError }}</p>
    </section>

    <main v-else class="dashboard-grid">
      <section class="message-card info-banner">
        <div>
          <p class="eyebrow">Stav session</p>
          <h2>{{ useMockData ? 'Demo data aktivne' : 'Realtime pripojenie aktivne' }}</h2>
        </div>
        <div class="status-summary">
          <div>
            <span class="session-label">Firebase Auth</span>
            <strong>{{ signedInEmail }}</strong>
          </div>
          <div>
            <span class="session-label">ESP synchronizácia</span>
            <strong :class="telemetryHealthyClass">{{ telemetryHealthyLabel }}</strong>
          </div>
          <div>
            <span class="session-label">Naposledy merané</span>
            <strong>{{ latestTelemetryLabel }}</strong>
          </div>
        </div>
      </section>

      <section class="devices-panel">
        <h2>Ovládanie spotrebičov</h2>
        <div v-if="!telemetryHealthy" class="message-card warning">
          <p>ESP nie je online. Ovládanie je zablokované kým sa nepripojí.</p>
        </div>
        <div class="device-grid">
          <DeviceCard
            v-for="deviceId in DEVICE_IDS"
            :key="deviceId"
            :device-id="deviceId"
            :device="devices[deviceId]"
            :timer="devices[deviceId].scheduledAction"
            :busy="actionBusy || !telemetryHealthy"
            :disabled="!telemetryHealthy"
            @toggle="toggleDevice(deviceId)"
            @brightness-change="updateBrightness(deviceId, $event)"
            @set-timer="setDeviceTimer(deviceId, $event.action, $event.durationMs, $event.brightness)"
            @cancel-timer="cancelDeviceTimer(deviceId)"
          />
        </div>
      </section>

      <section class="charts-panel">
        <h2>Spotreba za posledných 30 dní</h2>
        <div class="chart-grid">
          <ConsumptionChart
            :labels="monthlyData.device1.labels"
            :values="monthlyData.device1.values"
            color="#1f7a8c"
            title="LED - kWh"
          />
          <ConsumptionChart
            :labels="monthlyData.device2.labels"
            :values="monthlyData.device2.values"
            color="#bf4342"
            title="Fan - kWh"
          />
        </div>
      </section>
    </main>
  </div>
</template>
