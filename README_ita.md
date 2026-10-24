<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | 🇮🇹 <b>Italiano</b> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Un nodo ESP32-S3 che sta nel quadro di una macchina Bungard CCD, si collega alla porta RS-232 della sua unità di controllo e lascia che i programmi EH3D guidino la macchina via Ethernet o Wi-Fi, con le spie della macchina e il variatore del mandrino sulla stessa scheda

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Verifica di onestà - cosa funziona oggi:** un firmware in C++17 per l'ESP32-S3 (ESP-IDF 5.5) che trasforma la porta RS-232 dell'unità di controllo di una macchina Bungard CCD in una porta di rete: un programma si collega via TCP con un token di accesso, arma la macchina e invia i byte di un movimento, che il nodo mette in coda e porta sulla linea al ritmo della linea stessa. Legge anche le spie della macchina (HOME X, Y e Z, ENABLE, TOOL), comanda un mandrino con un segnale di velocità 0-10 V e linee di marcia e di senso, e ha un pannello web (otto lingue) per la messa in servizio, gli utenti, la rete, l'aggiornamento e le impostazioni proprie della macchina. Le regole del collegamento e le impostazioni sono provate su un computer, e il firmware si compila senza avvisi per entrambe le schede. **Non è ancora stato caricato né eseguito su una scheda**, e la scheda portante che riunisce il convertitore RS-232, gli ingressi delle spie e il variatore del mandrino è un progetto su carta.

---

> **Stato di sicurezza funzionale e informatica:** questo non è un sistema di sicurezza. Un nodo cablato male, o un variatore del mandrino impostato male, può muovere una macchina o far girare un utensile. Il nodo rifiuta di essere comandato senza un token di accesso, accetta byte per la macchina solo finché un programma lo ha armato, e si ferma (coda svuotata, driver dei motori e mandrino spenti) quando la sessione tace o cade la connessione (a meno che sia attiva l'impostazione facoltativa che gli lascia finire il lavoro che ha). Vede i finecorsa della macchina solo tramite le spie, e non ha ancora un arresto di emergenza proprio (la scheda portante prevede un ingresso per un pulsante). Il variatore del mandrino lavora a tensione di rete: cablalo e impostalo con il manuale del costruttore. Il pannello non muove mai la macchina e rifiuta di salvare le impostazioni finché un programma tiene la macchina armata. Il progetto è indipendente: non è prodotto, approvato né supportato da Bungard Elektronik, e i nomi dei prodotti compaiono solo per dire con quali macchine e variatori deve funzionare.

## 📋 In breve

| | |
|---|---|
| Versione | `0.0.2` |
| Maturità | Impalcatura |
| Test | 9 suite di test di host (circa 2200 verifiche) |
| Stack tecnologico | C++17, ESP-IDF 5.5, ESP32-S3 |
| Su hardware reale | No: le due immagini del firmware si compilano, ma nulla è ancora girato su una scheda |

## 🚦 Stato del progetto

Ogni riga è una parte di questo repository e il semaforo dice a che punto è oggi. È scritta in base a ciò che il repository può mostrare (test, compilazioni, esecuzioni), non a ciò che è previsto.

🟢 Fatto e verificato · 🟡 Funziona, con limiti noti · 🟠 Fatto a metà o non ancora verificato · 🔴 Non fatto o non funzionante

| Area | Stato | Cosa significa qui |
|---|---|---|
| Software | 🟡 | Il firmware per l'ESP32-S3 (ESP-IDF 5.5, C++17) compila senza avvisi per entrambe le schede, Ethernet e Wi-Fi; nessuna immagine è stata scritta. |
| Test | 🟢 | 9 programmi di test sul PC passano (`ctest`), circa 2.200 controlli: il collegamento, le impostazioni, le regole della macchina, la base condivisa, i testi del pannello e la sicurezza dei task. |
| Collegamento di controllo | 🟡 | Il protocollo a frame (CRC, accesso con token, armamento, coda di byte, arresto quando la sessione tace) è coperto dai test sul PC; nessun programma ha parlato con un nodo reale. |
| Pannello web | 🟡 | Il pannello web (otto lingue: configurazione, utenti, rete, impostazioni, aggiornamento) ha i testi verificati ed è provato contro una simulazione del nodo; non è mai stato servito da una scheda. |
| Mandrino e spie | 🟠 | Il segnale di velocità 0-10 V, le linee di marcia e di senso (nessuna inversione durante la rotazione) e i cinque ingressi delle spie sono testati sul PC; nessun azionamento né macchina è stato collegato. |
| Aggiornamenti | 🟠 | Il caricamento in uno dei due slot firmware con ripristino e conferma è testato sul PC; un aggiornamento non è mai stato eseguito su una scheda. |
| Sicurezza | 🟡 | Codice di configurazione sulla console USB, token di accesso, utenti e ruoli, HTTPS facoltativo e niente di segreto compilato; si ferma da solo se il collegamento cade. Non ha ancora un ingresso di arresto di emergenza né una revisione indipendente. |
| Hardware reale | 🔴 | Nessuna scheda ha eseguito questo firmware, e la scheda portante con il convertitore RS-232, gli ingressi delle spie e l'azionamento del mandrino è solo un progetto su carta. |
| Documentazione | 🟢 | README in otto lingue, il protocollo, il firmware, il cablaggio e il progetto della scheda portante. |

## 🎯 Panoramica

* Il **collegamento di controllo** (TCP, porta 5800 predefinita): un protocollo a frame con CRC, un accesso con token, l'armamento, una coda di byte per l'unità di controllo e un arresto quando la sessione tace o cade. Il programma invia i byte di un movimento già spaziati e la UART del nodo li porta sulla linea con esattezza, cosa che un adattatore USB-seriale non riesce a fare ad alta velocità.
* La **linea seriale verso l'unità di controllo**: 38400 baud 8N1 tramite un MAX3232, con le linee DTR, DCD e RI dell'handshake. I pin, e se il transceiver inverte le linee, sono impostazioni.
* Le **spie**: HOME X, HOME Y, HOME Z, ENABLE e TOOL sono lette tramite un transistor separatore, mostrate nello stato e sul pannello, e una richiesta può far scartare al nodo il resto di una ricerca appena un asse è a casa, così il programma non deve più leggere la linea di home in mezzo al flusso.
* Il **mandrino**: un PWM di velocità per un convertitore 0-10 V (24000 giri/min a segnale pieno per impostazione predefinita, per un variatore di frequenza come l'Huanyang HY02D223B) e linee di marcia avanti / indietro, senza cambio di senso mentre gira né subito dopo l'arresto, e arresto ogni volta che la macchina viene fermata.
* Il **pannello** (HTTP e HTTPS, otto lingue): messa in servizio con un codice mostrato sulla console USB, utenti, Ethernet o Wi-Fi, il collegamento di controllo, i pin RS-232, le spie e il mandrino, un registro, e aggiornamenti per caricamento in uno di due slot di firmware con ripristino. Nell'immagine non si compila nulla di segreto.
* L'**autoprotezione**: una guardia che sorveglia i task e la memoria, un cancello davanti al pannello quando manca memoria, e la conferma di una nuova immagine quando il nodo è raggiungibile.
* Le **schede**: la Waveshare ESP32-S3-ETH (Ethernet W5500) e una ESP32-S3-WROOM-1 N16R8 (Wi-Fi), un'immagine per ciascuna. Il progetto della scheda portante (100 x 100 mm) è in `docs/BOARD.md`.
* Le **macchine**: la Bungard CCD MTC e la CCD/2 MTC, con l'unità di controllo su porta seriale del software DOS 4.6. I due modelli ATC non sono coperti; le XL e le macchine successive hanno una propria unità di controllo moderna.
* La **fine autonoma** (disattivata per impostazione predefinita, è un'impostazione): quando il programma o il server scompare, il nodo esegue fino in fondo i byte già in coda - compresi i comandi del mandrino intercalati - poi spegne il mandrino e i driver. La coda vive nella RAM esterna (fino a 4 MB, minuti di movimento); la termina un limite di tempo, un Abort e il pulsante di arresto del pannello. **Previsto, non ancora fatto**: un ingresso per un pulsante di arresto fisico.

## 🧩 Ruolo in EH3D

EH3D-NODE-BUNGARD è l'ultimo stadio tra i programmi EH3D e la macchina: EH3D-DRILLPRO ed EH3D-ROUTEPRO vi si collegano oggi, ed EH3D-SERVER si metterà davanti. È il primo nodo della famiglia EH3D e le sue impostazioni, il suo pannello e il sistema di aggiornamento sono pensati per essere condivisi con i nodi che seguiranno (la base comune vive in EH3D-COMMON).

## 📂 Struttura del repository

```text
EH3D-NODE-BUNGARD/
├── core/         i frame e le regole di sessione del collegamento, la coda di byte e le impostazioni: senza hardware, provati su un computer
├── main/         il firmware ESP-IDF: UART, collegamento di controllo, spie, mandrino, rete, archivio delle impostazioni, server web
├── panel/        il pannello web, integrato nel firmware
├── tests/        test di host e verifiche dei testi del pannello
├── tools/        build_node.sh, pack_panel.py, check_sdkconfig.py, ci_validate.py, panel_mock.mjs
├── CMakeLists.txt, partitions.csv, sdkconfig.defaults, sdkconfig.board.*  il progetto ESP-IDF, la mappa della flash e le impostazioni di ogni scheda (s3-eth, s3-wifi)
├── docs/         PROTOCOL, NODE_FIRMWARE, WIRING e BOARD
├── images/       banner
└── eh3d.project.json
```

## 🛠️ COMPILARE ED ESEGUIRE

Servono un compilatore C++17 e CMake per i test di host, ed ESP-IDF 5.5 (o Docker) per il firmware.

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth` (o `s3-wifi`) costruisce un'immagine nel container ESP-IDF; su Windows un ESP-IDF 5.5.5 installato fa lo stesso. L'immagine `dist/eh3d-node-bungard-<versione>-<scheda>-complete.bin` è per UNA scheda: caricala all'indirizzo 0 con `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <immagine>`. Poi collegati al Wi-Fi `EH3D-SETUP-xxxxxx` che il nodo apre, apri `http://192.168.4.1/` e inserisci il codice di messa in servizio che il nodo mostra sulla sua console USB; il token di accesso, la rete e i pin si impostano dal pannello.

## 🔗 Progetti correlati

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): il programma di foratura; guida la macchina tramite questo nodo con `connection = node`
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): il programma di fresatura; condivide la libreria di protocollo e il file della macchina
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): le parti comuni della famiglia EH3D; la base del firmware del nodo vive lì
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): il centro della famiglia EH3D: esegue i lavori dei programmi e degli altri client sulla macchina tramite questo nodo

## 📚 Documentazione e community

* [Il collegamento di controllo](docs/PROTOCOL.md): frame, accesso, armamento, coda, mandrino, spie e le regole che fermano la macchina.
* [Il firmware](docs/NODE_FIRMWARE.md): messa in servizio, impostazioni, pannello, aggiornamento con ripristino, memoria e autoprotezione.
* [Cablaggio](docs/WIRING.md): la porta RS-232, le spie e il variatore del mandrino.
* [La scheda portante](docs/BOARD.md): il progetto da cui disegnare lo schema.
* [Registro delle modifiche](CHANGELOG.md)
* [Contribuire](CONTRIBUTING.md)
* [Politica di sicurezza](SECURITY.md)
* [Supporto](SUPPORT.md)

## 👤 AUTORE

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENZA

Software: GPL-3.0-or-later. Vedi [LICENSE](LICENSE). I progetti hardware della suite usano la CERN OHL-S.
