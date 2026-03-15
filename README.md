# IoT Spotreba Domacnosti (Vue + Firebase)

Single page web aplikacia vo Vue 3, ktora obsahuje:

- prihlasenie/registraciu cez Firebase Auth
- ovladanie 2 spotrebicov (zapnut/vypnut)
- realtime monitoring vykonu (W)
- graf spotreby za poslednych 30 dni pre kazdy spotrebic samostatne
- responzivne UI pre desktop aj mobil

## 1. Spustenie projektu

1. Nainstaluj zavislosti:

```bash
npm install
```

2. Vytvor `.env` podla `.env.example` a dopln hodnoty z Firebase Console.

3. Spusti vyvojovy server:

```bash
npm run dev
```

4. Build produkcie:

```bash
npm run build
```

## 2. Firebase konfiguracia

Pouziva sa Firebase klientsky SDK vo fronteende. To znamena:

- `apiKey` a dalsie `VITE_` hodnoty su klientsky konfig a mozu byt vo fronteende
- bezpecnost sa riesi cez Firebase Auth + Realtime Database Security Rules
- nikdy nedavaj do frontendu Service Account private key

### Firebase moduly, ktore treba zapnut

- Authentication: Email/Password
- Realtime Database

## 3. Struktura dat v Realtime Database

Odporucana struktura:

```json
{
	"devices": {
		"device1": {
			"name": "Spotrebic A",
			"isOn": true,
			"powerW": 125.4,
			"updatedAt": 1760000000000
		},
		"device2": {
			"name": "Spotrebic B",
			"isOn": false,
			"powerW": 0,
			"updatedAt": 1760000000000
		}
	},
	"consumptionLogs": {
		"device1": {
			"logId1": {
				"timestamp": 1760000000000,
				"energyWh": 55.2
			}
		},
		"device2": {
			"logId2": {
				"timestamp": 1760000000000,
				"energyWh": 81.7
			}
		}
	}
}
```

`consumptionLogs` moze mat `energyWh` alebo `kWh`. UI podporuje obe moznosti.

## 4. Realtime Database Security Rules (zaklad)

```json
{
	"rules": {
		".read": "auth != null",
		".write": "auth != null",
		"devices": {
			"$deviceId": {
				".validate": "$deviceId === 'device1' || $deviceId === 'device2'"
			}
		},
		"consumptionLogs": {
			"$deviceId": {
				".validate": "$deviceId === 'device1' || $deviceId === 'device2'"
			}
		}
	}
}
```

## 5. Integracia Arduino

- Arduino (alebo most medzi Arduino a Firebase) zapisuje:
	- aktualny vykon do `devices/{deviceId}/powerW`
	- stav do `devices/{deviceId}/isOn` (ak ma byt zdielany stav)
	- historicke merania do `consumptionLogs/{deviceId}/{logId}`
- Web aplikacia meni `isOn`; Arduino tuto hodnotu cita a vykona zapnutie/vypnutie rele.

## 6. Poznamka k adresaru iot-bc

V pracovnom priecinku sa mohol vytvorit aj podadresar `iot-bc` pri prvotnom scaffolde. Aktivny projekt je v koreni workspace.
