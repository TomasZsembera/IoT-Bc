<script setup>
import { Chart, CategoryScale, LinearScale, LineController, LineElement, PointElement, Tooltip, Filler, Legend } from 'chart.js'
import { onMounted, onUnmounted, ref, watch } from 'vue'

Chart.register(CategoryScale, LinearScale, LineController, LineElement, PointElement, Tooltip, Filler, Legend)

const props = defineProps({
  title: {
    type: String,
    required: true,
  },
  labels: {
    type: Array,
    required: true,
  },
  values: {
    type: Array,
    required: true,
  },
  color: {
    type: String,
    default: '#1f7a8c',
  },
})

const canvasRef = ref(null)
let chart = null

function buildChart() {
  if (!canvasRef.value) {
    return
  }

  chart = new Chart(canvasRef.value, {
    type: 'line',
    data: {
      labels: props.labels,
      datasets: [
        {
          label: props.title,
          data: props.values,
          borderColor: props.color,
          backgroundColor: `${props.color}33`,
          fill: true,
          tension: 0.25,
          pointRadius: 2,
        },
      ],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: {
          display: false,
        },
      },
      scales: {
        y: {
          title: {
            display: true,
            text: 'kWh',
          },
          beginAtZero: true,
        },
      },
    },
  })
}

watch(
  () => [props.labels, props.values],
  () => {
    if (!chart) {
      return
    }
    chart.data.labels = props.labels
    chart.data.datasets[0].data = props.values
    chart.update()
  },
  { deep: true },
)

onMounted(buildChart)

onUnmounted(() => {
  if (chart) {
    chart.destroy()
  }
})
</script>

<template>
  <article class="chart-card">
    <h3>{{ title }}</h3>
    <div class="canvas-wrap">
      <canvas ref="canvasRef" />
    </div>
  </article>
</template>

<style scoped>
.chart-card {
  background: #ffffff;
  border: 1px solid #d5e3e8;
  border-radius: 18px;
  padding: 1rem;
  min-height: 320px;
  width: 100%;
  min-width: 0;
  box-shadow: 0 12px 24px rgba(10, 35, 56, 0.08);
}

h3 {
  margin: 0 0 0.8rem;
  font-size: 1rem;
}

.canvas-wrap {
  width: 100%;
  height: 260px;
  overflow: hidden;
}
</style>
