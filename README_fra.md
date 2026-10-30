<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | 🇫🇷 <b>Français</b> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Un nœud ESP32-S3 qui se place dans le boîtier d'une machine Bungard CCD, se branche sur le port RS-232 de son unité de commande et laisse les programmes EH3D piloter la machine par Ethernet ou Wi-Fi, avec les voyants de la machine et le variateur de la broche sur la même carte

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.3-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Vérification d'honnêteté - ce qui tourne aujourd'hui :** firmware en C++17 pour l'ESP32-S3 (ESP-IDF 5.5) qui transforme le port RS-232 de l'unité de commande d'une machine Bungard CCD en port réseau : un programme se connecte en TCP avec un jeton d'accès, arme la machine et envoie les octets d'un mouvement, que le nœud met en file et envoie sur la ligne au rythme propre de la ligne. Il lit aussi les voyants de la machine (HOME X, Y et Z, ENABLE, TOOL), pilote une broche par un signal de vitesse 0-10 V et des lignes de marche et de sens, et possède un panneau web avec menu latéral (huit langues) pour la mise en service, les utilisateurs, le réseau, la mise à jour et les réglages propres à la machine. Les règles du lien et les réglages sont testés sur un ordinateur (9 programmes de test, 2209 vérifications comptées) et le firmware se compile sans avertissement pour les deux cartes. **Il n'a pas encore été flashé ni exécuté sur une carte**, rien n'a été branché à une machine, et la carte porteuse qui réunit le convertisseur RS-232, les entrées des voyants et le variateur de la broche est un projet sur papier.

---

> **État de la sûreté et de la sécurité :** ceci n'est pas un système de sûreté. Un nœud mal câblé, ou un variateur de broche mal réglé, peut déplacer une machine ou faire tourner un outil. Le nœud refuse d'être piloté sans jeton d'accès, n'accepte des octets pour la machine que pendant qu'un programme l'a armé, et s'arrête (file vidée, drivers de moteurs et broche coupés) quand la session se tait ou que la connexion tombe (sauf si le réglage facultatif qui lui laisse finir le travail qu'il détient est activé). Il ne voit les fins de course de la machine que par les voyants et n'a pas encore d'arrêt d'urgence propre (la carte porteuse a une place pour une entrée de bouton-poussoir). Le variateur de la broche est sous tension secteur : câble-le et règle-le avec le manuel du fabricant. Le panneau ne déplace jamais la machine et refuse d'enregistrer des réglages tant qu'un programme tient la machine armée. Le projet est indépendant : il n'est ni fabriqué, ni approuvé, ni soutenu par Bungard Elektronik, et les noms de produits n'apparaissent que pour dire avec quelles machines et quels variateurs il est prévu de fonctionner.

## 📋 En un coup d'œil

| | |
|---|---|
| Version | `0.0.3` |
| Maturité | Échafaudage |
| Tests | 9 programmes de test sur ordinateur (2209 vérifications comptées) |
| Pile technique | C++17, ESP-IDF 5.5, ESP32-S3 |
| Sur du matériel réel | Non : les deux images de firmware se compilent, mais rien n'a été flashé ni exécuté sur une carte et rien n'a été branché à une machine |

**Aller à  :** [Présentation](#-présentation) · [Architecture](#-architecture) · [Démarrage rapide](#-démarrage-rapide) · [Tests](#-tests) · [Structure](#-structure-du-dépôt) · [Liés](#-projets-liés)

## 🚦 État du projet

Chaque ligne est une partie de ce dépôt et le feu dit où elle en est aujourd'hui. Elle est écrite d'après ce que le dépôt peut montrer (tests, compilations, exécutions), pas d'après ce qui est prévu.

🟢 Fait et vérifié · 🟡 Fonctionne, avec des limites connues · 🟠 À moitié fait ou pas encore vérifié · 🔴 Pas fait ou ne fonctionne pas

| Domaine | État | Ce que cela veut dire ici |
|---|---|---|
| Logiciel | 🟡 | Le firmware pour l'ESP32-S3 (ESP-IDF 5.5, C++17) se compile sans avertissement pour les deux cartes, Ethernet et Wi-Fi ; aucune image n'a été flashée. |
| Tests | 🟢 | 9 programmes de test sur ordinateur réussissent (`ctest`) : 2209 vérifications comptées sur le lien, les réglages, les règles de la machine et la base partagée, plus le contrôle des textes du panneau (315 lignes en 8 langues) et des piles des tâches (10 tâches). |
| Lien de commande | 🟡 | Le protocole par trames (CRC, connexion par jeton, armement, file d'octets, arrêt quand la session se tait) est couvert par les tests sur ordinateur ; aucun programme n'a parlé à un vrai nœud. |
| Panneau web | 🟡 | Le panneau web (menu latéral, huit langues : mise en service, machine, réseau, utilisateurs, mise à jour, journal, système) a ses textes contrôlés et est exercé contre une maquette du nœud dans un navigateur ; il n'a jamais été servi depuis une carte. |
| Broche et voyants | 🟠 | Le signal de vitesse 0-10 V, les lignes de marche et de sens (pas d'inversion pendant la rotation) et les cinq entrées de voyants sont testés sur l'ordinateur ; aucun variateur ni aucune machine n'a été branché. |
| Mises à jour | 🟠 | Le téléversement dans l'un des deux emplacements de firmware avec retour arrière et confirmation est testé sur l'ordinateur ; une mise à jour n'a jamais tourné sur une carte. |
| Sécurité | 🟡 | Code de mise en service sur la console USB, jeton d'accès, utilisateurs et rôles, HTTPS facultatif et rien de secret compilé ; il s'arrête de lui-même quand le lien tombe. Il n'a pas encore d'entrée d'arrêt d'urgence ni de revue indépendante. |
| Matériel réel | 🔴 | Aucune carte n'a exécuté ce firmware, et la carte porteuse avec le convertisseur RS-232, les entrées de voyants et le variateur de la broche n'est qu'un projet sur papier. |
| Documentation | 🟢 | README en huit langues, le protocole, le firmware, le câblage et le projet de la carte porteuse. |

## 🎯 Présentation

* Le **lien de commande** (TCP, port 5800 par défaut) : un protocole par trames avec CRC, une connexion avec jeton d'accès, l'armement, une file d'octets pour l'unité de commande et un arrêt quand la session se tait ou tombe. Le programme envoie les octets d'un mouvement déjà espacés et l'UART du nœud les place exactement sur la ligne, ce qu'un adaptateur USB-série ne peut pas faire à vitesse élevée.
* La **ligne série vers l'unité de commande** : 38400 bauds 8N1 par un MAX3232, avec les lignes DTR, DCD et RI de la poignée de main. Les broches, et le fait que l'émetteur-récepteur inverse ou non les lignes, sont des réglages.
* Les **voyants** : HOME X, HOME Y, HOME Z, ENABLE et TOOL sont lus par un transistor tampon, affichés dans l'état et sur le panneau, et une requête peut faire abandonner au nœud le reste d'une recherche dès qu'un axe est en butée, si bien que le programme n'a plus à lire la ligne home au milieu du flux.
* La **broche** : un PWM de vitesse pour un convertisseur 0-10 V (24000 tr/min à signal maximal par défaut, pour un variateur de fréquence comme le Huanyang HY02D223B) et des lignes de marche avant / arrière, sans changement de sens pendant la rotation ni juste après l'arrêt, et un arrêt chaque fois que la machine est arrêtée.
* Le **panneau** (HTTP et HTTPS, huit langues, le menu à gauche sur un écran large et en haut sur un téléphone) : mise en service avec un code affiché sur la console USB, utilisateurs, Ethernet ou Wi-Fi, le lien de commande, les broches RS-232, les voyants et la broche, un journal et des mises à jour par téléversement dans l'un des deux emplacements de firmware avec retour arrière. Rien de secret n'est compilé dans l'image.
* L'**autoprotection** : un gardien qui surveille les tâches et la mémoire, une porte à l'entrée du panneau quand la mémoire manque, et la confirmation d'une nouvelle image dès que le nœud est joignable.
* Les **cartes** : la Waveshare ESP32-S3-ETH (Ethernet W5500) et une ESP32-S3-WROOM-1 N16R8 (Wi-Fi), une image pour chacune. Le projet de la carte porteuse (100 x 100 mm) est dans `docs/BOARD.md`.
* Les **machines** : la Bungard CCD MTC et la CCD/2 MTC, avec l'unité de commande à port série du logiciel DOS 4.6. Les deux modèles ATC ne sont pas couverts ; les modèles XL et les machines suivantes ont une unité de commande moderne qui leur est propre.
* La **fin autonome** (désactivée par défaut, un réglage) : quand le programme ou le serveur disparaît, le nœud exécute jusqu'au bout les octets déjà dans sa file - les commandes de broche en file comprises - puis coupe la broche et les drivers. La file vit dans la RAM externe (jusqu'à 4 Mo, des minutes de mouvement), une limite de temps y met fin, de même qu'un Abort et le bouton d'arrêt du panneau. **Prévu, pas encore construit** : une entrée de bouton-poussoir pour un arrêt physique.

## 🧩 Rôle dans EH3D

EH3D-NODE-BUNGARD est le dernier étage entre les programmes EH3D et la machine : EH3D-DRILLPRO et EH3D-ROUTEPRO s'y connectent aujourd'hui, et EH3D-SERVER se place devant pour les autres clients. C'est le premier nœud de la famille EH3D et ses réglages, son panneau et sa mécanique de mise à jour sont faits pour être partagés avec les nœuds suivants (la base commune vit dans EH3D-COMMON).

## 🏛️ Architecture

<p align="center">
  <img src="images/architecture.svg" alt="Architecture de EH3D-NODE-BUNGARD : qui l'utilise, ce qu'il contient et de quoi il dépend" width="100%">
</p>

Comment les parties de ce dépôt s'articulent : qui l'utilise, ce qu'il contient et de quoi il dépend. L'image est dessinée à partir de `docs/architecture.json` et vérifiée par rapport au code.

## 🖼️ Captures d'écran

<p align="center">
  <img src="images/screenshots/panel-status.png" alt="status page of the web panel" width="100%">
</p>

<p align="center"><em>La page État du panneau web, servie par le nœud de remplacement avec lequel on développe : aucune carte n'a encore exécuté le firmware.</em></p>

<p align="center">
  <img src="images/screenshots/panel-machine.png" alt="machine page of the web panel" width="100%">
</p>

<p align="center"><em>La page Machine : la liaison de contrôle, les broches RS-232, les voyants et le jeton d'accès.</em></p>

## ✨ Fonctionnalités

* **Un panneau avec menu latéral :** sept pages (État, Machine, Réseau, Utilisateurs, Firmware, Journal, Système), le menu à gauche sur un écran large et en haut sur un téléphone, avec la langue, le thème et la déconnexion à côté.
* **Une image, pas de secrets :** tout ce qui change d'un nœud à l'autre est un document de réglages en flash modifié depuis le panneau ; aucun mot de passe, jeton ni réseau n'est compilé.
* **Des octets au bon rythme :** le programme place chaque pas et l'UART du nœud envoie les octets au rythme propre de la ligne, ce qu'un adaptateur USB ne sait pas tenir.
* **Home vu en direct :** les cinq voyants sont lus et une requête abandonne le reste d'une recherche dès qu'un axe est en butée.
* **Broche avec garde-fous :** vitesse plafonnée à la vitesse maximale pour laquelle le nœud a été construit, pas d'inversion pendant la rotation ni juste après l'arrêt, arrêt avec la machine.
* **Mise à jour avec retour arrière :** une image va dans l'emplacement qui ne tourne pas et reste à l'essai tant que le nœud n'est pas joignable ; le chargeur de démarrage revient en arrière quand il ne l'est pas.
* **Il veille sur lui-même :** un gardien surveille les tâches et la mémoire, et un nœud sans adresse après quatre-vingt-dix secondes rouvre le réseau de mise en service pour ne pas rester injoignable.

## 🔒 Sécurité et protection

| Menace | Défense |
|---|---|
| Quelqu'un sur le réseau pilote la machine | le lien refuse tout sauf Hello tant que le jeton d'accès (8 à 64 caractères imprimables) n'est pas bon ; un nœud sans jeton refuse le pilotage sauf si le réglage d'établi `allow_open` est activé (désactivé par défaut) ; un programme à la fois |
| Un programme qui déplace la machine par accident | les octets ne sont acceptés que pendant l'armement, et seulement si la trame entière tient dans la file ; une commande de broche est ramenée à la vitesse maximale et refusée si elle inversait une broche qui tourne |
| Un câble qui se débranche ou un programme qui meurt au milieu d'un mouvement | une session armée qui se tait pendant `idle_ms` (3000 ms par défaut) ou dont la connexion tombe vide la file et envoie l'octet de drivers coupés `0xC0` ; la broche passe à 0 tr/min |
| Quelqu'un atteint le panneau | le premier administrateur est créé avec un code de mise en service qui n'apparaît que sur la console USB ; deux rôles (administrateur, lecteur) ; un mauvais mot de passe est ralenti et une adresse source qui continue d'échouer est bloquée un temps ; HTTP, HTTPS ou les deux |
| Des secrets dans le firmware ou le dépôt | rien de secret n'est compilé : le jeton, le réseau et les broches sont des réglages en flash ; le jeton est en écriture seule dans le panneau et l'API |
| Une écriture de réglages qui bloque un mouvement | tant qu'un programme tient la machine armée, le panneau refuse d'enregistrer des réglages (HTTP 409 `machine_armed`) et reporte un redémarrage |
| Une mauvaise affectation de broches | les broches que la carte réserve (flash, PSRAM, USB, la puce Ethernet) sont refusées et aucune broche ne peut faire deux travaux |
| Une mauvaise image de firmware | une image d'un autre projet ou un emplacement vide est refusé ; une nouvelle image est à l'essai et le chargeur de démarrage revient à la précédente si le panneau ne démarre pas ou si le nœud n'est pas joignable en cinq minutes |
| Un nœud qui manque de mémoire ou se bloque | un gardien surveille les tâches et la mémoire interne et redémarre sans former de boucle ; une porte à l'entrée du panneau quand la mémoire manque |
| Quelqu'un qui a le nœud entre les mains | maintenir BOOT huit secondes dans les trente premières après la mise sous tension efface les réglages et les utilisateurs (l'accès physique est une réinitialisation par conception) |

**Ce qui n'est pas défendu :** le nœud n'a pas encore d'entrée d'arrêt d'urgence et ne voit les fins de course que par les voyants ; la mise à jour n'est pas signée (la mise à jour depuis GitHub n'est pas construite : elle exige une clé de signature) ; le certificat fait maison du panneau HTTPS fait avertir un navigateur ; rien n'a tourné sur une carte et il n'y a pas eu de revue indépendante. Signale une vulnérabilité en privé (voir [SECURITY.md](SECURITY.md)).

## 🔌 Interfaces

| Interface | Ce que c'est | Où |
|---|---|---|
| Lien de commande | TCP, port 5800 par défaut : trames `0xB5, type, longueur, charge, CRC-16`, Hello avec le jeton, Arm, Write, Abort, Ping, StopOnHome, Spindle ; le nœud répond Info, Status (toutes les 200 ms), Rx, HomeSeen et Error | [docs/PROTOCOL.md](docs/PROTOCOL.md), `core/frame.hpp`, `main/link_server.cpp` |
| Panneau web | HTTP sur le port 80 et HTTPS sur le 443 (le mode est un réglage), huit langues, API sous `/api/v1/` (`GET`/`PUT /api/v1/machine`, `GET /api/v1/machine/status` et les routes partagées de la base) | [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md), `main/web_server.cpp`, `panel/` |
| RS-232 vers l'unité de commande | UART à 38400 bauds 8N1 par un MAX3232, avec DTR (sortie), DCD et RI (entrées) | [docs/WIRING.md](docs/WIRING.md), `main/central_uart.cpp` |
| Voyants | HOME X, HOME Y, HOME Z, ENABLE et TOOL, cinq entrées par un transistor tampon | [docs/WIRING.md](docs/WIRING.md), `main/lamps.cpp` |
| Broche | un PWM de vitesse pour un convertisseur 0-10 V et des lignes de marche avant / arrière | [docs/BOARD.md](docs/BOARD.md), `main/spindle.cpp` |
| Console USB | affiche le code de mise en service et accepte Improv Wi-Fi par série | `main/improv_serial.cpp` |
| Réseau de mise en service | le nœud ouvre le Wi-Fi `EH3D-SETUP-xxxxxx` tant qu'il n'a pas d'utilisateur | [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md) |

## ⚙️ Configuration

Tout est un réglage en flash, modifié depuis le panneau (page Machine), si bien qu'une seule image sert pour chaque nœud. Les valeurs ci-dessous sont celles d'un nœud jamais configuré ; une valeur hors plage est refusée à l'enregistrement.

| Réglage | Défaut | Plage | Ce qu'il fait |
|---|---|---|---|
| `machine.link.port` | 5800 | de 1024 à 65535 | le port TCP du lien de commande |
| `machine.link.token` | aucun | de 8 à 64 caractères imprimables ; écriture seule | le jeton d'accès ; sans jeton, le nœud refuse d'être piloté |
| `machine.link.allow_open` | désactivé | activé ou désactivé | un réglage d'établi : accepter le pilotage sans jeton |
| `machine.link.idle_ms` | 3000 | de 200 à 60000 | combien de temps une session armée peut se taire avant que le nœud s'arrête |
| `machine.link.queue_bytes` | 262144 | de 1024 à 4194304 | la file d'octets pour l'unité de commande, dans la RAM externe |
| `machine.link.finish_on_loss` | désactivé | activé ou désactivé | finir le travail déjà dans la file quand le programme disparaît |
| `machine.link.max_drain_s` | 600 | de 10 à 3600 | le nombre maximal de secondes que le nœud peut continuer seul dans ce cas |
| `machine.uart.*` | tx 17, rx 18, dtr 15, dcd 16, ri 21, cts et dsr non câblés ; l'émetteur-récepteur inverse | une broche libre de la carte, ou -1 si facultatif | les broches de la ligne série |
| `machine.lamps.*` | HOME X 1, Y 2, Z 7, ENABLE 8, TOOL 38 ; actif au niveau bas | une broche libre | les cinq entrées de voyants |
| `machine.spindle.max_rpm` | 24000 | de 0 à 65535 (0 : pas de sortie de broche) | la vitesse à signal maximal |
| `machine.spindle.pwm_hz` | 2000 | de 100 à 40000 | la fréquence du signal de vitesse |
| `machine.spindle.reverse_delay_ms` | 3000 | de 0 à 60000 | combien de temps la broche doit être arrêtée avant de pouvoir tourner dans l'autre sens |
| `web.mode` | `both` | `http`, `both` ou `https` | si le panneau est servi sur le port 80, sur le 443 ou sur les deux |

Les broches de la broche (PWM 4, avant 5, arrière 6) sont aussi des réglages. Les réglages que tout nœud possède (nom, accès, point d'accès, adresse, horloge, langue, redémarrage périodique) sont dans la base et décrits dans [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md).

## 🚀 Démarrage rapide

Il faut un compilateur C++17 et CMake pour les tests sur ordinateur, et ESP-IDF 5.5 (ou Docker) pour le firmware.

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth` (ou `s3-wifi`) compile une image dans le conteneur ESP-IDF ; sous Windows, un ESP-IDF 5.5.5 installé fait de même. L'image `dist/eh3d-node-bungard-<version>-<carte>-complete.bin` est pour UNE carte : flashe-la à l'adresse 0 avec `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <image>`. Rejoins ensuite le Wi-Fi `EH3D-SETUP-xxxxxx` que le nœud ouvre, ouvre `http://192.168.4.1/` et saisis le code de mise en service que le nœud affiche sur sa console USB ; le jeton d'accès, le réseau et les broches se règlent depuis le panneau. Ce chemin n'a pas encore été suivi sur une vraie carte.

## 🧪 Tests

Les tests sur ordinateur (`build.bat` ou `./build.sh`, puis `ctest`) sont 9 programmes et tous réussissent : `core_tests` (104 vérifications : les trames et les règles de session), `test_base` (382), `test_clock` (affiche ok), `test_settings` (392), `test_machine` (75), `test_shared` (627) et `test_shared_eth` (629, le profil de la carte Ethernet) - 2209 vérifications comptées - plus `panel_text` (315 lignes en 8 langues, 265 clés utilisées, 36 codes d'erreur du firmware avec un texte) et `task_safety` (10 tâches, aucune grosse copie des réglages sur une petite pile). Ils couvrent le CRC et le décodeur qui se resynchronise après un dégât, la connexion par jeton, l'armement, l'arrêt sur silence, les règles de la broche (plafond, pas d'inversion, délai), les réglages et leurs plages, les conflits de broches et les broches réservées, utilisateurs et sessions, le plan réseau, l'horloge et JSON. Le panneau a aussi été exercé dans un navigateur contre `tools/panel_mock.mjs`, un nœud de substitution qui le sert sans carte. La CI exécute `python -I tools/ci_validate.py` et les tests sur ordinateur sous Linux avec les avertissements en erreurs. La compilation des deux images de firmware (ESP-IDF 5.5.5) se fait à la main et ne fait pas partie de la CI ; rien ne tourne sur une carte.

## 🏗️ Compilation et publication

La version se trouve dans `eh3d.project.json` et dans la compilation, et chaque version a son entrée dans [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` compilent et exécutent les tests sur ordinateur et `test.sh` / `test.bat` n'exécutent que les tests, chacun dans un panneau en direct ; aucun ne change la version. Les images de firmware viennent de `tools/build_node.sh` (ou d'ESP-IDF 5.5.5 sous Windows) et s'appellent `eh3d-node-bungard-<version>-<carte>-{app,complete}.bin`. Aucune version n'a encore été publiée et aucune image n'a été flashée. La mise à jour depuis GitHub n'est pas construite : elle exige une clé de signature.

## 🩺 Dépannage

| Symptôme | Que faire |
|---|---|
| Le nœud est injoignable | Un nœud sans adresse après quatre-vingt-dix secondes rouvre le réseau de mise en service `EH3D-SETUP-xxxxxx` ; rejoins-le et règle l'accès depuis le panneau. |
| Le code de mise en service est perdu | Il n'apparaît que sur la console USB, toutes les quinze secondes. Maintenir BOOT huit secondes dans les trente premières après la mise sous tension efface les réglages et les utilisateurs et relance la mise en service. |
| Le lien répond Error 1 ou 2 | Pas connecté, ou mauvais jeton : la première trame doit être Hello avec le jeton d'accès réglé dans le panneau (page Machine). |
| Error 3 ou 4 | Pas armé (envoie d'abord Arm) ou la file est pleine (attends que Status montre de la place). |
| Error 7 | La session armée s'est tue pendant `idle_ms` : envoie Ping tant qu'un long mouvement est en file. |
| Error 9, 10 ou 11 | La broche doit s'arrêter avant d'inverser, elle était arrêtée depuis trop peu de temps pour inverser, ou le nœud finit le travail qu'il détient. |
| Le panneau refuse d'enregistrer (409 `machine_armed`) | Un programme tient la machine armée ; désarme-la ou appuie sur le bouton d'arrêt de la page État, puis enregistre à nouveau. |
| Le navigateur avertit à propos du panneau HTTPS | Le nœud fabrique son propre certificat la première fois ; utilise le mode `http` ou `both` sur un réseau de confiance, ou accepte le certificat. |

## 📂 Structure du dépôt

<p align="center">
  <img src="images/repository-structure.svg" alt="Structure du dépôt EH3D-NODE-BUNGARD" width="100%">
</p>

<details>
<summary>Version texte de l'arborescence</summary>

```text
EH3D-NODE-BUNGARD/
├── core/
│   ├── frame.hpp session.hpp                    trames et règles de session
│   ├── byte_queue.hpp drain.hpp                 file d'octets, fin du travail
│   ├── bungard_config.hpp                       réglages machine et broches
│   ├── node_settings.hpp auth.hpp               réglages, utilisateurs (base)
│   └── netplan.hpp json.hpp                     plan réseau et JSON (base)
├── main/
│   ├── app_main.cpp link_server.cpp             démarrage et lien de commande
│   ├── central_uart.cpp lamps.cpp               ligne série et voyants
│   ├── spindle.cpp machine_api.cpp              sorties broche, API
│   ├── web_server.cpp network.cpp               serveur du panneau, réseau (base)
│   └── node_store.cpp node_guard.cpp            stockage, gardien (base)
├── panel/
│   ├── index.html style.css                     la page et la disposition du menu latéral
│   └── app.js text.js                           les pages et les textes en huit langues
├── tests/                                       tests sur ordinateur du lien, des réglages, de la base partagée et des textes du panneau
├── docs/                                        protocole, firmware, câblage, carte
├── tools/                                       compilation, panneau, contrôles, nœud factice
├── CMakeLists.txt                               le projet ESP-IDF
├── partitions.csv                               la flash à deux emplacements
├── sdkconfig.*                                  réglages ESP-IDF, profils
├── images/                                      bannière et images d'Architecture et de Structure du dépôt
├── .github/                                     workflows de CI et de publication, modèles d'issues et de pull requests
├── build.sh build.bat                           compile et exécute les tests sur ordinateur dans un panneau en direct
├── eh3d.project.json                            manifeste : version, rôle, maturité et commande de compilation
├── CHANGELOG.md                                 ce qui a changé dans chaque version
├── SECURITY.md SUPPORT.md                       signaler une vulnérabilité et poser des questions
└── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   contribuer, code de conduite et licence
```

</details>

## 🗺️ Feuille de route et limites connues

**Limites connues aujourd'hui**

* Rien n'a tourné sur une carte : aucune image n'a été flashée, aucune mise en service faite sur un vrai nœud, aucun programme ne lui a parlé et rien n'a été branché à une machine.
* La carte porteuse avec le convertisseur RS-232, les entrées de voyants et le variateur de la broche est un projet sur papier (`docs/BOARD.md`).
* Le nœud n'a pas d'entrée d'arrêt d'urgence (la carte porteuse a une place pour un bouton-poussoir) et ne voit les fins de course que par les voyants.
* La mise à jour depuis GitHub n'est pas construite : elle exige une clé de signature. Le panneau a été vu dans un navigateur contre un nœud de substitution, pas servi depuis une carte.
* La fin du travail retenu n'a pas tourné sur une carte ; ses règles sont testées sur un ordinateur.
* Seules les machines Bungard CCD MTC et CCD/2 MTC sont visées ; les modèles ATC ne sont pas couverts.

**Ensuite**

* Flasher une image sur chaque carte, faire la première mise en service et vérifier le lien avec la bibliothèque Rust.
* Dessiner et construire la carte porteuse, et brancher un variateur de broche et une machine.
* Ajouter l'entrée de bouton-poussoir pour un arrêt physique.
* Signer les mises à jour et les récupérer depuis GitHub.

## 🌍 Langues

Le README et les textes existent en anglais, espagnol, français, italien, allemand, chinois, japonais et portugais européen.

## 🔗 Projets liés

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO) : le programme de perçage ; il pilote la machine par ce nœud avec `connection = node`
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO) : le programme de défonçage ; il partage la bibliothèque du protocole et le fichier de machine
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON) : les parties communes de la famille EH3D ; la base du firmware du nœud y vit
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER) : le centre de la famille EH3D : il exécute les travaux des programmes et des autres clients sur la machine par ce nœud

## 📚 Documentation

* [Le lien de commande](docs/PROTOCOL.md) : trames, connexion, armement, file, broche, voyants et règles qui arrêtent la machine.
* [Le firmware](docs/NODE_FIRMWARE.md) : mise en service, réglages, panneau, mise à jour avec retour arrière, mémoire et autoprotection.
* [Câblage](docs/WIRING.md) : le port RS-232, les voyants et le variateur de la broche.
* [La carte porteuse](docs/BOARD.md) : le projet à partir duquel dessiner le schéma.
* [Journal des modifications](CHANGELOG.md)

## 🤝 Contribuer et communauté

* Avant une pull request, exécute les tests sur ordinateur (`./build.sh` ou `build.bat`) et `python -I tools/ci_validate.py` : tout doit réussir. Voir [CONTRIBUTING.md](CONTRIBUTING.md) et le [code de conduite](CODE_OF_CONDUCT.md).
* Le README existe en huit langues : une modification du texte se fait dans les huit fichiers, et les deux images sont dessinées à partir de `docs/architecture.json` et `docs/structure.json` par `tools/make_architecture.py` et `tools/make_structure.py`.
* Rien de ce qui envoie des octets à une vraie unité de commande ne change sans une note de ce qui a été observé sur le matériel (ou à l'analyseur logique) et un test avec le simulateur ou le nœud de substitution.
* Signalez une vulnérabilité en privé : [SECURITY.md](SECURITY.md). Questions, idées et rapports : electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTEUR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCE

Logiciel : GPL-3.0-or-later. Voir [LICENSE](LICENSE). Les conceptions matérielles de la suite utilisent la CERN OHL-S.
