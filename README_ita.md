<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | 🇮🇹 <b>Italiano</b> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Un nodo ESP32-S3 che sta nella scatola di una macchina Bungard CCD, si collega alla porta RS-232 della sua unità di controllo e lascia che i programmi EH3D guidino la macchina via Ethernet o Wi-Fi, con i LED della macchina e il variatore del mandrino sulla stessa scheda

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.3-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Verifica di onestà - cosa funziona oggi:** firmware in C++17 per l'ESP32-S3 (ESP-IDF 5.5) che trasforma la porta RS-232 dell'unità di controllo di una macchina Bungard CCD in una porta di rete: un programma si connette via TCP con un token di accesso, arma la macchina e invia i byte di un movimento, che il nodo mette in coda e porta sulla linea al ritmo proprio della linea. Legge anche i LED della macchina (HOME X, Y e Z, ENABLE, TOOL), pilota un mandrino con un segnale di velocità 0-10 V e linee di marcia e direzione, e ha un pannello web con menu laterale (otto lingue) per la configurazione iniziale, gli utenti, la rete, l'aggiornamento e le impostazioni della macchina. Le regole del collegamento e le impostazioni sono provate su un computer (9 programmi di test, 2209 controlli contati) e il firmware compila senza avvisi per entrambe le schede. **Non è ancora stato scritto né eseguito su una scheda**, nulla è stato collegato a una macchina, e la scheda portante che riunisce il convertitore RS-232, gli ingressi dei LED e il variatore del mandrino è un progetto su carta.

---

> **Stato di sicurezza funzionale e informatica:** questo non è un sistema di sicurezza funzionale. Un nodo cablato male, o un variatore del mandrino impostato male, può muovere una macchina o far girare un utensile. Il nodo rifiuta di essere controllato senza un token di accesso, accetta byte per la macchina solo mentre un programma l'ha armata e si ferma (coda svuotata, driver dei motori e mandrino spenti) quando la sessione tace o la connessione cade (a meno che sia attiva l'impostazione facoltativa che gli lascia finire il lavoro che tiene). Non vede i finecorsa della macchina se non tramite i LED e non ha ancora un arresto di emergenza proprio (la scheda portante ha un posto per un ingresso a pulsante). Il variatore del mandrino è a tensione di rete: cablalo e impostalo con il manuale del costruttore. Il pannello non muove mai la macchina e rifiuta di salvare le impostazioni finché un programma tiene la macchina armata. Il progetto è indipendente: non è prodotto, approvato né supportato da Bungard Elektronik, e i nomi dei prodotti compaiono solo per dire con quali macchine e variatori è pensato per funzionare.

## 📋 In breve

| | |
|---|---|
| Versione | `0.0.3` |
| Maturità | Impalcatura |
| Test | 9 programmi di test su computer (2209 controlli contati) |
| Stack tecnologico | C++17, ESP-IDF 5.5, ESP32-S3 |
| Su hardware reale | No: le due immagini del firmware compilano, ma nulla è stato scritto né eseguito su una scheda e nulla è stato collegato a una macchina |

**Vai a:** [Panoramica](#-panoramica) · [Architettura](#-architettura) · [Avvio rapido](#-avvio-rapido) · [Test](#-test) · [Struttura](#-struttura-del-repository) · [Correlati](#-progetti-correlati)

## 🚦 Stato del progetto

Ogni riga è una parte di questo repository e il semaforo dice a che punto è oggi. È scritta in base a ciò che il repository può mostrare (test, compilazioni, esecuzioni), non a ciò che è previsto.

🟢 Fatto e verificato · 🟡 Funziona, con limiti noti · 🟠 Fatto a metà o non ancora verificato · 🔴 Non fatto o non funzionante

| Area | Stato | Cosa significa qui |
|---|---|---|
| Software | 🟡 | Il firmware per l'ESP32-S3 (ESP-IDF 5.5, C++17) compila senza avvisi per entrambe le schede, Ethernet e Wi-Fi; nessuna immagine è stata scritta. |
| Test | 🟡 | Passano 9 programmi di test su computer (`ctest`): 2209 controlli contati su collegamento, impostazioni, regole della macchina e base condivisa, più il controllo dei testi del pannello (315 righe in 8 lingue) e degli stack dei task (10 task). |
| Collegamento di controllo | 🟡 | Il protocollo a frame (CRC, login con token, armamento, coda di byte, stop quando la sessione tace) è coperto dai test su computer; nessun programma ha parlato con un nodo reale. |
| Pannello web | 🟡 | Il pannello web (menu laterale, otto lingue: configurazione, macchina, rete, utenti, aggiornamento, registro, sistema) ha i testi controllati ed è provato contro una simulazione del nodo in un browser; non è mai stato servito da una scheda. |
| Mandrino e LED | 🟠 | Il segnale di velocità 0-10 V, le linee di marcia e direzione (nessuna inversione mentre gira) e i cinque ingressi dei LED sono provati sul computer; nessun variatore né macchina è stato collegato. |
| Aggiornamenti | 🟠 | Il caricamento in uno dei due slot del firmware con ripristino e conferma è provato sul computer; un aggiornamento non è mai stato eseguito su una scheda. |
| Sicurezza | 🟡 | Codice di configurazione sulla console USB, token di accesso, utenti e ruoli, HTTPS facoltativo e nulla di segreto compilato; si ferma da solo quando il collegamento cade. Non ha ancora un ingresso di arresto di emergenza né una revisione indipendente. |
| Hardware reale | 🔴 | Nessuna scheda ha eseguito questo firmware, e la scheda portante con il convertitore RS-232, gli ingressi dei LED e il variatore del mandrino è solo un progetto su carta. |
| Documentazione | 🟡 | README in otto lingue, il protocollo, il firmware, il cablaggio e il progetto della scheda portante. |

## 🎯 Panoramica

* Il **collegamento di controllo** (TCP, porta 5800 per impostazione predefinita): un protocollo a frame con CRC, un login con token di accesso, l'armamento, una coda di byte per l'unità di controllo e uno stop quando la sessione tace o cade. Il programma invia i byte di un movimento già distanziati e l'UART del nodo li mette sulla linea con esattezza, cosa che un adattatore USB-seriale non sa fare ad alta velocità.
* La **linea seriale verso l'unità di controllo**: 38400 baud 8N1 tramite un MAX3232, con le linee DTR, DCD e RI dell'handshake. I pin e se il ricetrasmettitore inverte le linee sono impostazioni.
* I **LED**: HOME X, HOME Y, HOME Z, ENABLE e TOOL vengono letti tramite un transistor buffer, mostrati nello stato e sul pannello, e una richiesta può far abbandonare al nodo il resto di una ricerca appena un asse è a casa, così il programma non deve più leggere la linea di home in mezzo al flusso.
* Il **mandrino**: un PWM di velocità per un convertitore 0-10 V (24000 giri/min a segnale pieno per impostazione predefinita, per un variatore di frequenza come l'Huanyang HY02D223B) e linee di marcia avanti / indietro, senza cambio di senso mentre gira o subito dopo l'arresto, e uno stop ogni volta che la macchina viene fermata.
* Il **pannello** (HTTP e HTTPS, otto lingue, il menu a sinistra su uno schermo largo e in alto su un telefono): configurazione iniziale con un codice mostrato sulla console USB, utenti, Ethernet o Wi-Fi, il collegamento di controllo, i pin RS-232, i LED e il mandrino, un registro e aggiornamenti tramite caricamento in uno dei due slot del firmware con ripristino. Nessun segreto è compilato nell'immagine.
* L'**autoprotezione**: un guardiano che osserva i task e la memoria, un cancello alla porta del pannello quando la memoria scarseggia e la conferma di una nuova immagine appena il nodo è raggiungibile.
* Le **schede**: la Waveshare ESP32-S3-ETH (Ethernet W5500) e una ESP32-S3-WROOM-1 N16R8 (Wi-Fi), un'immagine per ciascuna. Il progetto della scheda portante (100 x 100 mm) è in `docs/BOARD.md`.
* Le **macchine**: la Bungard CCD MTC e la CCD/2 MTC, con l'unità di controllo a porta seriale del software DOS 4.6. I due modelli ATC non sono coperti; i modelli XL e le macchine successive hanno una propria unità di controllo moderna.
* La **fine autonoma** (disattivata per impostazione predefinita, un'impostazione): quando il programma o il server sparisce, il nodo esegue fino in fondo i byte già in coda - compresi i comandi del mandrino in coda - poi spegne mandrino e driver. La coda vive nella RAM esterna (fino a 4 MB, minuti di movimento), un limite di tempo la termina e così un Abort e il pulsante di stop del pannello. **Previsto, non ancora costruito**: un ingresso a pulsante per uno stop fisico.

## 🧩 Ruolo in EH3D

EH3D-NODE-BUNGARD è l'ultimo stadio tra i programmi EH3D e la macchina: EH3D-DRILLPRO ed EH3D-ROUTEPRO vi si collegano oggi, ed EH3D-SERVER si pone davanti per gli altri client. È il primo nodo della famiglia EH3D e le sue impostazioni, il suo pannello e il meccanismo di aggiornamento sono pensati per essere condivisi con i nodi successivi (la base comune vive in EH3D-COMMON).

## 🏛️ Architettura

<p align="center">
  <img src="images/architecture.svg" alt="Architettura di EH3D-NODE-BUNGARD: chi lo usa, che cosa contiene e da che cosa dipende" width="100%">
</p>

Come si incastrano le parti di questo repository: chi lo usa, che cosa contiene e da che cosa dipende. L'immagine è disegnata da `docs/architecture.json` e verificata rispetto al codice.

## 🖼️ Schermate

<p align="center">
  <img src="images/screenshots/panel-status.png" alt="panel-status" width="100%">
</p>

<p align="center"><em>La pagina Stato, nel tema Forge degli altri programmi EH3D: il collegamento di controllo, le spie della macchina, le linee dell'unità di controllo e il mandrino. È servita dal nodo sostitutivo con cui si sviluppa il pannello: nessuna scheda ha ancora eseguito il firmware.</em></p>

<p align="center">
  <img src="images/screenshots/panel-machine.png" alt="panel-machine" width="100%">
</p>

<p align="center"><em>La pagina Macchina: il collegamento di controllo, i pin RS-232, le spie e il token di accesso.</em></p>

<p align="center">
  <img src="images/screenshots/panel-network.png" alt="panel-network" width="100%">
</p>

<p align="center"><em>La pagina Rete: come il nodo raggiunge la rete.</em></p>

<p align="center">
  <img src="images/screenshots/panel-users.png" alt="panel-users" width="100%">
</p>

<p align="center"><em>La pagina Utenti: le persone che possono aprire il pannello e il loro ruolo.</em></p>

<p align="center">
  <img src="images/screenshots/panel-firmware.png" alt="panel-firmware" width="100%">
</p>

<p align="center"><em>La pagina Firmware: i due slot, quello in esecuzione e l'aggiornamento con ripristino.</em></p>

<p align="center">
  <img src="images/screenshots/panel-system.png" alt="panel-system" width="100%">
</p>

<p align="center"><em>La pagina Sistema: l'identità del nodo, il riavvio e il ripristino di fabbrica.</em></p>

## ✨ Funzionalità

* **Un pannello con menu laterale:** sette pagine (Stato, Macchina, Rete, Utenti, Firmware, Registro, Sistema), il menu a sinistra su uno schermo largo e in alto su un telefono, con lingua, tema e uscita accanto.
* **Un'immagine, nessun segreto:** tutto ciò che cambia da un nodo all'altro è un documento di impostazioni in flash modificato dal pannello; nessuna password, token o rete è compilata.
* **Byte con la giusta temporizzazione:** il programma colloca ogni passo e l'UART del nodo invia i byte al ritmo proprio della linea, cosa che un adattatore USB non sa mantenere.
* **Home visto dal vivo:** i cinque LED vengono letti e una richiesta abbandona il resto di una ricerca appena un asse è a casa.
* **Mandrino con protezioni:** velocità limitata a quella massima per cui il nodo è stato costruito, nessuna inversione mentre gira o subito dopo l'arresto, stop insieme alla macchina.
* **Aggiornamento con ripristino:** un'immagine va nello slot non in uso ed è in prova finché il nodo non è raggiungibile; il bootloader torna indietro quando non lo è.
* **Si prende cura di sé:** un guardiano osserva i task e la memoria, e un nodo senza indirizzo dopo novanta secondi riapre la rete di configurazione per non restare irraggiungibile.

## 🔒 Sicurezza e protezione

| Minaccia | Difesa |
|---|---|
| Qualcuno in rete guida la macchina | il collegamento rifiuta tutto tranne Hello finché il token di accesso (da 8 a 64 caratteri stampabili) non è giusto; un nodo senza token rifiuta il controllo a meno che sia attiva l'impostazione da banco `allow_open` (disattivata per impostazione predefinita); un programma alla volta |
| Un programma che muove la macchina per errore | i byte sono accettati solo con il nodo armato e solo se l'intero frame entra nella coda; un comando del mandrino viene limitato alla velocità massima e rifiutato se invertirebbe un mandrino in rotazione |
| Un cavo che si stacca o un programma che muore a metà movimento | una sessione armata che tace per `idle_ms` (3000 ms per impostazione predefinita) o la cui connessione cade svuota la coda e invia il byte di driver spenti `0xC0`; il mandrino va a 0 giri/min |
| Qualcuno raggiunge il pannello | il primo amministratore si crea con un codice di configurazione che compare solo sulla console USB; due ruoli (amministratore, osservatore); una password sbagliata viene rallentata e un indirizzo sorgente che continua a fallire viene bloccato per un po'; HTTP, HTTPS o entrambi |
| Segreti nel firmware o nel repository | nulla di segreto è compilato: token, rete e pin sono impostazioni in flash; il token è di sola scrittura nel pannello e nell'API |
| Una scrittura di impostazioni che blocca un movimento | finché un programma tiene la macchina armata il pannello rifiuta di salvare le impostazioni (HTTP 409 `machine_armed`) e rimanda un riavvio |
| Un'assegnazione di pin sbagliata | i pin che la scheda riserva (flash, PSRAM, USB, il chip Ethernet) sono rifiutati e nessun pin può fare due lavori |
| Un'immagine di firmware difettosa | un'immagine di un altro progetto o uno slot vuoto è rifiutato; una nuova immagine è in prova e il bootloader torna alla precedente se il pannello non parte o il nodo non è raggiungibile entro cinque minuti |
| Un nodo che finisce la memoria o si blocca | un guardiano osserva i task e la memoria interna e riavvia senza creare un ciclo; un cancello alla porta del pannello quando la memoria scarseggia |
| Qualcuno ha il nodo tra le mani | tenere premuto BOOT otto secondi nei primi trenta dopo l'accensione cancella impostazioni e utenti (l'accesso fisico è un reset per progetto) |

**Ciò che non è difeso:** il nodo non ha ancora un ingresso di arresto di emergenza e non vede i finecorsa se non tramite i LED; l'aggiornamento non è firmato (l'aggiornamento da GitHub non è costruito: richiede una chiave di firma); il certificato autoprodotto del pannello HTTPS fa avvisare il browser; nulla è stato eseguito su una scheda e non c'è stata una revisione indipendente. Segnala una vulnerabilità in privato (vedi [SECURITY.md](SECURITY.md)).

## 🔌 Interfacce

| Interfaccia | Che cos'è | Dove |
|---|---|---|
| Collegamento di controllo | TCP, porta 5800 per impostazione predefinita: frame `0xB5, tipo, lunghezza, payload, CRC-16`, Hello con il token, Arm, Write, Abort, Ping, StopOnHome, Spindle; il nodo risponde Info, Status (ogni 200 ms), Rx, HomeSeen ed Error | [docs/PROTOCOL.md](docs/PROTOCOL.md), `core/frame.hpp`, `main/link_server.cpp` |
| Pannello web | HTTP sulla porta 80 e HTTPS sulla 443 (la modalità è un'impostazione), otto lingue, API sotto `/api/v1/` (`GET`/`PUT /api/v1/machine`, `GET /api/v1/machine/status` e le route condivise della base) | [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md), `main/web_server.cpp`, `panel/` |
| RS-232 verso l'unità di controllo | UART a 38400 baud 8N1 tramite un MAX3232, con DTR (uscita), DCD e RI (ingressi) | [docs/WIRING.md](docs/WIRING.md), `main/central_uart.cpp` |
| LED | HOME X, HOME Y, HOME Z, ENABLE e TOOL, cinque ingressi tramite un transistor buffer | [docs/WIRING.md](docs/WIRING.md), `main/lamps.cpp` |
| Mandrino | un PWM di velocità per un convertitore 0-10 V e linee di marcia avanti / indietro | [docs/BOARD.md](docs/BOARD.md), `main/spindle.cpp` |
| Console USB | mostra il codice di configurazione e accetta Improv Wi-Fi via seriale | `main/improv_serial.cpp` |
| Rete di configurazione | il nodo apre il Wi-Fi `EH3D-SETUP-xxxxxx` finché non ha utenti | [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md) |

## ⚙️ Configurazione

Tutto è un'impostazione in flash, modificata dal pannello (pagina Macchina), così un'unica immagine serve per ogni nodo. I valori qui sotto sono quelli di un nodo mai configurato; un valore fuori intervallo viene rifiutato al salvataggio.

| Impostazione | Predefinito | Intervallo | Cosa fa |
|---|---|---|---|
| `machine.link.port` | 5800 | da 1024 a 65535 | la porta TCP del collegamento di controllo |
| `machine.link.token` | nessuno | da 8 a 64 caratteri stampabili; sola scrittura | il token di accesso; senza uno il nodo rifiuta di essere controllato |
| `machine.link.allow_open` | disattivato | attivato o disattivato | un'impostazione da banco: accetta il controllo senza token |
| `machine.link.idle_ms` | 3000 | da 200 a 60000 | quanto può tacere una sessione armata prima che il nodo si fermi |
| `machine.link.queue_bytes` | 262144 | da 1024 a 4194304 | la coda di byte per l'unità di controllo, nella RAM esterna |
| `machine.link.finish_on_loss` | disattivato | attivato o disattivato | finire il lavoro già in coda quando il programma sparisce |
| `machine.link.max_drain_s` | 600 | da 10 a 3600 | i secondi massimi che il nodo può proseguire da solo in quel caso |
| `machine.uart.*` | tx 17, rx 18, dtr 15, dcd 16, ri 21, cts e dsr non cablati; il ricetrasmettitore inverte | un pin libero della scheda, o -1 dove facoltativo | i pin della linea seriale |
| `machine.lamps.*` | HOME X 1, Y 2, Z 7, ENABLE 8, TOOL 38; attivi bassi | un pin libero | i cinque ingressi dei LED |
| `machine.spindle.max_rpm` | 24000 | da 0 a 65535 (0: nessuna uscita del mandrino) | la velocità a segnale pieno |
| `machine.spindle.pwm_hz` | 2000 | da 100 a 40000 | la frequenza del segnale di velocità |
| `machine.spindle.reverse_delay_ms` | 3000 | da 0 a 60000 | quanto il mandrino deve essere rimasto fermo prima di poter girare nell'altro senso |
| `web.mode` | `both` | `http`, `both` o `https` | se il pannello è servito sulla porta 80, sulla 443 o su entrambe |

Anche i pin del mandrino (PWM 4, avanti 5, indietro 6) sono impostazioni. Le impostazioni che ogni nodo ha (nome, accesso, access point, indirizzo, orologio, lingua, riavvio periodico) sono nella base e descritte in [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md).

## 🚀 Avvio rapido

Servono un compilatore C++17 e CMake per i test su computer, ed ESP-IDF 5.5 (o Docker) per il firmware.

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth` (o `s3-wifi`) compila un'immagine nel contenitore ESP-IDF; su Windows un ESP-IDF 5.5.5 installato fa lo stesso. L'immagine `dist/eh3d-node-bungard-<versione>-<scheda>-complete.bin` è per UNA scheda: scrivila all'indirizzo 0 con `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <immagine>`. Poi unisciti al Wi-Fi `EH3D-SETUP-xxxxxx` che il nodo apre, apri `http://192.168.4.1/` e inserisci il codice di configurazione che il nodo mostra sulla sua console USB; il token di accesso, la rete e i pin si impostano dal pannello. Questo percorso non è ancora stato seguito su una scheda reale.

## 🧪 Test

I test su computer (`build.bat` o `./build.sh`, poi `ctest`) sono 9 programmi e passano tutti: `core_tests` (104 controlli: i frame e le regole di sessione), `test_base` (382), `test_clock` (stampa ok), `test_settings` (392), `test_machine` (75), `test_shared` (627) e `test_shared_eth` (629, il profilo della scheda Ethernet) - 2209 controlli contati - più `panel_text` (315 righe in 8 lingue, 265 chiavi usate, 36 codici di errore del firmware con un testo) e `task_safety` (10 task, nessuna copia grande delle impostazioni su uno stack piccolo). Coprono il CRC e il decodificatore che si risincronizza dopo un danno, il login con token, l'armamento, lo stop per silenzio, le regole del mandrino (limite, nessuna inversione, ritardo), le impostazioni e i loro intervalli, i conflitti di pin e i pin riservati, utenti e sessioni, il piano di rete, l'orologio e JSON. Il pannello è stato anche provato in un browser contro `tools/panel_mock.mjs`, un nodo sostitutivo che lo serve senza scheda. La CI esegue `python -I tools/ci_validate.py` e i test su computer su Linux con gli avvisi come errori. La compilazione delle due immagini del firmware (ESP-IDF 5.5.5) si fa a mano e non fa parte della CI; nulla gira su una scheda.

## 🏗️ Compilazione e rilascio

La versione sta in `eh3d.project.json` e nella compilazione, e ogni versione ha la sua voce in [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` compilano ed eseguono i test su computer e `test.sh` / `test.bat` eseguono solo i test, ciascuno in un pannello dal vivo; nessuno cambia la versione. Le immagini del firmware vengono da `tools/build_node.sh` (o da ESP-IDF 5.5.5 su Windows) e si chiamano `eh3d-node-bungard-<versione>-<scheda>-{app,complete}.bin`. Non è ancora stata pubblicata alcuna versione né scritta alcuna immagine. L'aggiornamento da GitHub non è costruito: richiede una chiave di firma.

## 🩺 Risoluzione dei problemi

| Sintomo | Cosa fare |
|---|---|
| Il nodo non è raggiungibile | Un nodo senza indirizzo dopo novanta secondi riapre la rete di configurazione `EH3D-SETUP-xxxxxx`; uniscitici e imposta l'accesso dal pannello. |
| Il codice di configurazione è perso | Compare solo sulla console USB, ogni quindici secondi. Tenere premuto BOOT otto secondi nei primi trenta dopo l'accensione cancella impostazioni e utenti e riavvia la configurazione. |
| Il collegamento risponde Error 1 o 2 | Non autenticato, o token errato: il primo frame deve essere Hello con il token di accesso impostato nel pannello (pagina Macchina). |
| Error 3 o 4 | Non armato (invia prima Arm) o la coda è piena (attendi che Status mostri spazio). |
| Error 7 | La sessione armata ha taciuto per `idle_ms`: invia Ping finché un lungo movimento è in coda. |
| Error 9, 10 o 11 | Il mandrino deve fermarsi prima di invertire, era fermo da troppo poco per invertire, oppure il nodo sta finendo il lavoro che tiene. |
| Il pannello rifiuta di salvare (409 `machine_armed`) | Un programma tiene la macchina armata; disarmala o premi il pulsante di stop nella pagina Stato, e salva di nuovo. |
| Il browser avvisa sul pannello HTTPS | Il nodo crea il proprio certificato la prima volta; usa la modalità `http` o `both` su una rete fidata, o accetta il certificato. |

## 📂 Struttura del repository

<p align="center">
  <img src="images/repository-structure.svg" alt="Struttura del repository EH3D-NODE-BUNGARD" width="100%">
</p>

<details>
<summary>Versione testuale dell'albero</summary>

```text
EH3D-NODE-BUNGARD/
├── core/
│   ├── frame.hpp session.hpp                    frame e regole di sessione
│   ├── byte_queue.hpp drain.hpp                 coda di byte, fine lavoro
│   ├── bungard_config.hpp                       impostazioni macchina e pin
│   ├── node_settings.hpp auth.hpp               impostazioni, utenti (base)
│   └── netplan.hpp json.hpp                     piano di rete e JSON (base)
├── main/
│   ├── app_main.cpp link_server.cpp             avvio e collegamento
│   ├── central_uart.cpp lamps.cpp               linea seriale e LED
│   ├── spindle.cpp machine_api.cpp              uscite mandrino, API
│   ├── web_server.cpp network.cpp               server pannello, rete (base)
│   └── node_store.cpp node_guard.cpp            archivio, guardiano (base)
├── panel/
│   ├── index.html style.css                     la pagina e la disposizione del menu laterale
│   └── app.js text.js                           le pagine e i testi in otto lingue
├── tests/                                       test su computer del collegamento, delle impostazioni, della base condivisa e dei testi del pannello
├── docs/                                        protocollo, firmware, cablaggio, scheda
├── tools/                                       build, pannello, controlli, nodo finto
├── CMakeLists.txt                               il progetto ESP-IDF
├── partitions.csv                               la flash con due slot
├── sdkconfig.*                                  impostazioni ESP-IDF, profili
├── images/                                      banner e le immagini di Architettura e Struttura del repository
├── .github/                                     workflow di CI e di rilascio, modelli di issue e pull request
├── build.sh build.bat                           compila ed esegue i test su computer in un pannello dal vivo
├── eh3d.project.json                            manifesto: versione, ruolo, maturità e comando di compilazione
├── CHANGELOG.md                                 cosa è cambiato in ogni versione
├── SECURITY.md SUPPORT.md                       come segnalare una vulnerabilità e dove chiedere
└── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   come contribuire, codice di condotta e licenza
```

</details>

## 🗺️ Roadmap e limiti noti

**Limiti noti oggi**

* Nulla è stato eseguito su una scheda: nessuna immagine è stata scritta, nessuna configurazione fatta su un nodo reale, nessun programma gli ha parlato e nulla è stato collegato a una macchina.
* La scheda portante con il convertitore RS-232, gli ingressi dei LED e il variatore del mandrino è un progetto su carta (`docs/BOARD.md`).
* Il nodo non ha un ingresso di arresto di emergenza (la scheda portante ha un posto per un pulsante) e vede i finecorsa solo tramite i LED.
* L'aggiornamento da GitHub non è costruito: richiede una chiave di firma. Il pannello è stato visto in un browser contro un nodo sostitutivo, non servito da una scheda.
* La fine del lavoro trattenuto non è stata eseguita su una scheda; le sue regole sono provate su un computer.
* Sono previste solo le macchine Bungard CCD MTC e CCD/2 MTC; i modelli ATC non sono coperti.

**Prossimi passi**

* Scrivere un'immagine su ciascuna scheda, fare la prima configurazione e verificare il collegamento con la libreria Rust.
* Disegnare e costruire la scheda portante e collegare un variatore del mandrino e una macchina.
* Aggiungere l'ingresso a pulsante per uno stop fisico.
* Firmare gli aggiornamenti e scaricarli da GitHub.

## 🌍 Lingue

Il README e i testi esistono in inglese, spagnolo, francese, italiano, tedesco, cinese, giapponese e portoghese europeo.

## 🔗 Progetti correlati

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): il programma di foratura; guida la macchina tramite questo nodo con `connection = node`
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): il programma di fresatura; condivide la libreria del protocollo e il file macchina
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): le parti comuni della famiglia EH3D; lì vive la base del firmware del nodo
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): il centro della famiglia EH3D: esegue i lavori dei programmi e degli altri client sulla macchina tramite questo nodo

## 📚 Documentazione

* [Il collegamento di controllo](docs/PROTOCOL.md): frame, login, armamento, coda, mandrino, LED e le regole che fermano la macchina.
* [Il firmware](docs/NODE_FIRMWARE.md): configurazione iniziale, impostazioni, pannello, aggiornamento con ripristino, memoria e autoprotezione.
* [Cablaggio](docs/WIRING.md): la porta RS-232, i LED e il variatore del mandrino.
* [La scheda portante](docs/BOARD.md): il progetto da cui disegnare lo schema.
* [Registro delle modifiche](CHANGELOG.md)

## 🤝 Contribuire e comunità

* Prima di una pull request esegui i test su computer (`./build.sh` o `build.bat`) e `python -I tools/ci_validate.py`: tutto deve passare. Vedi [CONTRIBUTING.md](CONTRIBUTING.md) e il [codice di condotta](CODE_OF_CONDUCT.md).
* Il README esiste in otto lingue: una modifica al testo si fa in tutti e otto i file, e le due immagini sono disegnate da `docs/architecture.json` e `docs/structure.json` con `tools/make_architecture.py` e `tools/make_structure.py`.
* Nulla che invii byte a una vera unità di controllo cambia senza una nota di ciò che è stato osservato sull'hardware (o con un analizzatore logico) e un test con il simulatore o il nodo sostitutivo.
* Segnala una vulnerabilità in privato: [SECURITY.md](SECURITY.md). Domande, idee e segnalazioni: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTORE

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENZA

Software: GPL-3.0-or-later. Vedi [LICENSE](LICENSE). I progetti hardware della suite usano la CERN OHL-S.
