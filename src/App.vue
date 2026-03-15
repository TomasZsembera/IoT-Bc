<script setup>
import { computed, onMounted, onUnmounted, reactive, ref } from 'vue'
import {
  createUserWithEmailAndPassword,
  onAuthStateChanged,
  signInWithEmailAndPassword,
  signOut,
} from 'firebase/auth'
import { get, onValue, query, orderByChild, ref as dbRef, startAt, endAt, update } from 'firebase/database'
import DeviceCard from './components/DeviceCard.vue'
import ConsumptionChart from './components/ConsumptionChart.vue'
import { auth, db, isFirebaseConfigured, useMockData } from './firebase'

const DEVICE_IDS = ['device1', 'device2']
const DEVICE_LABELS = {
  device1: 'Spotrebic A',
  device2: 'Spotrebic B',
}

const authLoading = ref(true)
const authReady = ref(false)
const user = ref(null)
const isLoginMode = ref(true)
const email = ref('')
const password = ref('')
const authError = ref('')
const authBusy = ref(false)
const actionBusy = ref(false)

const devices = reactive({
  device1: { name: DEVICE_LABELS.device1, isOn: false, powerW: 0, updatedAt: 0 },
  device2: { name: DEVICE_LABELS.device2, isOn: false, powerW: 0, updatedAt: 0 },
})

const monthlyData = reactive({
  device1: { labels: [], values: [] },
  device2: { labels: [], values: [] },
})

let authUnsubscribe = null
const dbUnsubscribers = []
let mockInterval = null

const isLoggedIn = computed(() => !!user.value)

function normalizePowerW(value) {
  const numeric = Number(value)
  return Number.isFinite(numeric) ? Number(numeric.toFixed(2)) : 0
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

      const base = deviceIndex === 0 ? 1.1 : 1.6
      const wave = Math.sin((29 - i) / 4) * 0.25
      const jitter = Math.random() * 0.18
      values.push(Number((base + wave + jitter).toFixed(3)))
    }

    monthlyData[deviceId] = { labels, values }
  })
}

function applyMockDeviceValues() {
  DEVICE_IDS.forEach((deviceId, deviceIndex) => {
    const isOn = devices[deviceId].isOn
    const basePower = deviceIndex === 0 ? 180 : 260
    const fluctuation = Math.random() * 35

    devices[deviceId] = {
      ...devices[deviceId],
      powerW: isOn ? Number((basePower + fluctuation).toFixed(2)) : 0,
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

function listenToDevices() {
  clearRealtimeListeners()

  DEVICE_IDS.forEach((deviceId) => {
    const unsubscribe = onValue(dbRef(db, `devices/${deviceId}`), (snapshot) => {
      const value = snapshot.val() ?? {}
      devices[deviceId] = {
        name: value.name || DEVICE_LABELS[deviceId],
        isOn: Boolean(value.isOn),
        powerW: normalizePowerW(value.powerW),
        updatedAt: Number(value.updatedAt) || 0,
      }
    })

    dbUnsubscribers.push(unsubscribe)
  })
}

async function loadMonthlyData() {
  const now = Date.now()
  const thirtyDaysAgo = now - 30 * 24 * 60 * 60 * 1000

  for (const deviceId of DEVICE_IDS) {
    const totalsByDay = new Map()

    const logQuery = query(
      dbRef(db, `consumptionLogs/${deviceId}`),
      orderByChild('timestamp'),
      startAt(thirtyDaysAgo),
      endAt(now),
    )

    const snap = await get(logQuery)
    const raw = snap.val() || {}

    Object.values(raw).forEach((entry) => {
      if (!entry || typeof entry !== 'object') {
        return
      }

      const timestamp = Number(entry.timestamp)
      if (!Number.isFinite(timestamp)) {
        return
      }

      const fromKwh = Number(entry.kWh)
      const fromWh = Number(entry.energyWh)
      const kwh = Number.isFinite(fromKwh)
        ? fromKwh
        : Number.isFinite(fromWh)
          ? fromWh / 1000
          : 0

      if (kwh <= 0) {
        return
      }

      const dateKey = formatDateKey(timestamp)
      totalsByDay.set(dateKey, (totalsByDay.get(dateKey) || 0) + kwh)
    })

    const labels = []
    const values = []
    for (let i = 29; i >= 0; i -= 1) {
      const timestamp = now - i * 24 * 60 * 60 * 1000
      const key = formatDateKey(timestamp)
      labels.push(labelFromDateKey(key))
      values.push(Number((totalsByDay.get(key) || 0).toFixed(3)))
    }

    monthlyData[deviceId] = { labels, values }
  }
}

async function handleAuthSubmit() {
  authError.value = ''
  authBusy.value = true

  try {
    if (isLoginMode.value) {
      await signInWithEmailAndPassword(auth, email.value, password.value)
    } else {
      await createUserWithEmailAndPassword(auth, email.value, password.value)
    }
    password.value = ''
  } catch (error) {
    authError.value = 'Prihlasenie zlyhalo. Skontroluj email/heslo a Firebase Auth nastavenia.'
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
    devices[deviceId] = {
      ...devices[deviceId],
      isOn: !devices[deviceId].isOn,
      updatedAt: Date.now(),
    }
    applyMockDeviceValues()
    return
  }

  actionBusy.value = true
  try {
    const next = !devices[deviceId].isOn
    await update(dbRef(db, `devices/${deviceId}`), {
      isOn: next,
      updatedAt: Date.now(),
    })
  } finally {
    actionBusy.value = false
  }
}

onMounted(() => {
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
      await loadMonthlyData()
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

  clearRealtimeListeners()
  if (typeof authUnsubscribe === 'function') {
    authUnsubscribe()
  }
})
</script>

<template>
  <div class="page-shell">
    <header class="topbar">
      <div>
        <p class="eyebrow">IoT Energeticky Panel</p>
        <h1>Domace spotrebice v realnom case</h1>
      </div>
      <button v-if="isLoggedIn" class="secondary-btn" @click="handleLogout">Odhlasit sa</button>
    </header>

    <section v-if="!isFirebaseConfigured && !useMockData" class="message-card warning">
      <h2>Firebase konfiguracia chyba</h2>
      <p>
        Vytvor subor <strong>.env</strong> podla <strong>.env.example</strong> a dopln Firebase hodnoty z konzoly.
      </p>
    </section>

    <section v-else-if="authLoading" class="message-card">
      <p>Nacitavam autentifikaciu...</p>
    </section>

    <section v-else-if="!isLoggedIn && authReady" class="auth-card">
      <h2>{{ isLoginMode ? 'Prihlasenie' : 'Registracia' }}</h2>
      <p>Po prihlaseni mozes monitorovat spotrebu a ovladat oba spotrebice.</p>

      <form class="auth-form" @submit.prevent="handleAuthSubmit">
        <label>
          Email
          <input v-model="email" type="email" required autocomplete="email" />
        </label>

        <label>
          Heslo
          <input v-model="password" type="password" required minlength="6" autocomplete="current-password" />
        </label>

        <button class="primary-btn" :disabled="authBusy" type="submit">
          {{ authBusy ? 'Spracuvavam...' : isLoginMode ? 'Prihlasit sa' : 'Vytvorit ucet' }}
        </button>
      </form>

      <button class="link-btn" @click="isLoginMode = !isLoginMode">
        {{ isLoginMode ? 'Nemam ucet - registrovat' : 'Mam ucet - prihlasit' }}
      </button>

      <p v-if="authError" class="error-text">{{ authError }}</p>
    </section>

    <main v-else class="dashboard-grid">
      <section class="devices-panel">
        <h2>Ovladanie spotrebicov</h2>
        <div class="device-grid">
          <DeviceCard
            v-for="deviceId in DEVICE_IDS"
            :key="deviceId"
            :device="devices[deviceId]"
            :busy="actionBusy"
            @toggle="toggleDevice(deviceId)"
          />
        </div>
      </section>

      <section class="charts-panel">
        <h2>Spotreba za poslednych 30 dni</h2>
        <div class="chart-grid">
          <ConsumptionChart
            :labels="monthlyData.device1.labels"
            :values="monthlyData.device1.values"
            color="#1f7a8c"
            title="Spotrebic A - kWh"
          />
          <ConsumptionChart
            :labels="monthlyData.device2.labels"
            :values="monthlyData.device2.values"
            color="#bf4342"
            title="Spotrebic B - kWh"
          />
        </div>
      </section>
    </main>
  </div>
</template>
