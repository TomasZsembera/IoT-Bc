# IoT Spotreba Domacnosti (Vue + Firebase)

Single page web aplikacia vo Vue 3, ktora obsahuje:

- prihlasenie (bez registracie) cez Firebase Auth
- ovladanie 2 spotrebicov (zapnut/vypnut)
- ovladanie jasu pre LED (`device1`)
- realtime monitoring vykonu (W)
- graf spotreby za poslednych 30 dni pre kazdy spotrebic samostatne z `consumptionLogs`
- status synchronizacie ESP so stavom posledneho logu
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
- prihlasenie je dostupne iba pre uzivatelov, ktorych manualne pridas v Firebase Console (sekcii Authentication)
- registracia je vypnutá; novy ucet si moze vytvorit len spravca
- local `.env` je nastaveny na `VITE_USE_MOCK_DATA=false`; demo režim nechaj len na offline testovanie.

### Firebase moduly, ktore treba zapnut

- Authentication: Email/Password
- Realtime Database

## 3. Struktura dat v Realtime Database

Odporucana struktura:

```json
{
	"devices": {
		"device1": {
			"name": "LED",
			"isOn": true,
			"powerW": 125.4,
			"brightness": 60,
			"updatedAt": 1760000000000
		},
		"device2": {
			"name": "Fan",
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

Pre LED odporucam drzat `brightness` v rozsahu `0-100`. Arduino alebo ESP32 potom vie tento percentualny jas premapovat na PWM.

Pre plny realtime stav je vhodne, aby firmware pri kazdom odoslani telemetrie aktualizoval `devices/{deviceId}/updatedAt`. Ak to este nemas, web pouzije ako stav synchronizacie posledny log z `consumptionLogs`.

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

Ak chces mierne prisnejsi setup, mozes write obmedzit len na tvoj backend alebo na zname UID a pre web UI nechat len citanie.

## 5. Integracia Arduino

- Arduino (alebo most medzi Arduino a Firebase) zapisuje:
	- aktualny vykon do `devices/{deviceId}/powerW`
	- stav do `devices/{deviceId}/isOn` (ak ma byt zdielany stav)
	- jas LED do `devices/device1/brightness`
	- historicke merania do `consumptionLogs/device1/{logId}` a `consumptionLogs/device2/{logId}`
- Web aplikacia meni `isOn`; Arduino tuto hodnotu cita a vykona zapnutie/vypnutie rele.
- Pri LED Arduino citaj `brightness` a prevadzaj ho na PWM.

### Poznamka k tvojmu firmware flow

- ATmega posiela surove AC aj DC hodnoty na ESP.
- ESP zapisuje okamzity vykon a priebezne pushuje logy do Realtime Database.
- Pre web je najdolezitejsie, aby sa logy pushovali pre obe zariadenia a aby sa v `updatedAt` objavil posledny cas synchronizacie.
- Ak chces, mozem ti v dalsom kroku upravit aj tie dva firmware bloky tak, aby zapisovali `updatedAt`, `device1` logy a korektne mapovali LED brightness na PWM.

## 6. Poznamka k adresaru iot-bc

V pracovnom priecinku sa mohol vytvorit aj podadresar `iot-bc` pri prvotnom scaffolde. Aktivny projekt je v koreni workspace.
