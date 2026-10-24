<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | 🇫🇷 <b>Français</b> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Un nœud ESP32-S3 qui se place dans le coffret d'une machine Bungard CCD, se branche sur le port RS-232 de son unité de commande et laisse les programmes EH3D piloter la machine par Ethernet ou Wi-Fi, avec les voyants de la machine et le variateur de la broche sur la même carte

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Vérification d'honnêteté - ce qui tourne aujourd'hui :** un micrologiciel en C++17 pour l'ESP32-S3 (ESP-IDF 5.5) qui transforme le port RS-232 de l'unité de commande d'une machine Bungard CCD en port réseau : un programme se connecte en TCP avec un jeton d'accès, arme la machine et envoie les octets d'un déplacement, que le nœud met en file et sort sur la ligne au rythme de la ligne elle-même. Il lit aussi les voyants de la machine (HOME X, Y et Z, ENABLE, TOOL), pilote une broche avec un signal de vitesse de 0-10 V et des lignes de marche et de sens, et a un panneau web (huit langues) pour la mise en service, les utilisateurs, le réseau, la mise à jour et les réglages propres à la machine. Les règles de la liaison et les réglages sont testés sur un ordinateur, et le micrologiciel se compile sans avertissement pour les deux cartes. **Il n'a pas encore été flashé ni exécuté sur une carte**, et la carte porteuse qui réunit le convertisseur RS-232, les entrées des voyants et le variateur de la broche n'est qu'un projet sur le papier.

---

> **État de la sûreté et de la sécurité :** ceci n'est pas un système de sécurité. Un nœud mal câblé, ou un variateur de broche mal réglé, peut déplacer une machine ou faire tourner un outil. Le nœud refuse d'être piloté sans jeton d'accès, n'accepte des octets pour la machine que tant qu'un programme l'a armé, et s'arrête (file vidée, drivers des moteurs et broche coupés) quand la session se tait ou que la connexion tombe (sauf si le réglage optionnel qui lui laisse finir le travail qu'il détient est activé). Il ne voit les fins de course de la machine que par les voyants, et n'a pas encore d'arrêt d'urgence propre (la carte porteuse prévoit une entrée de bouton). Le variateur de la broche est sur la tension secteur : câblez-le et réglez-le avec le manuel du fabricant. Le panneau ne déplace jamais la machine, et refuse d'enregistrer des réglages tant qu'un programme tient la machine armée. Le projet est indépendant : il n'est ni fabriqué, ni approuvé, ni soutenu par Bungard Elektronik, et les noms de produits n'apparaissent que pour dire avec quelles machines et quels variateurs il doit fonctionner.

## 📋 En un coup d'œil

| | |
|---|---|
| Version | `0.0.2` |
| Maturité | Échafaudage |
| Tests | 9 suites de tests d'hôte (environ 2200 vérifications) |
| Pile technique | C++17, ESP-IDF 5.5, ESP32-S3 |
| Sur du matériel réel | Non : les deux images du micrologiciel se compilent, mais rien n'a encore tourné sur une carte |

## 🎯 Vue d'ensemble

* La **liaison de commande** (TCP, port 5800 par défaut) : un protocole à trames avec CRC, une connexion par jeton d'accès, l'armement, une file d'octets pour l'unité de commande et un arrêt quand la session se tait ou tombe. Le programme envoie les octets d'un déplacement déjà espacés et l'UART du nœud les met sur la ligne avec exactitude, ce qu'un adaptateur USB-série ne sait pas faire à vitesse élevée.
* La **ligne série vers l'unité de commande** : 38400 bauds 8N1 par un MAX3232, avec les lignes DTR, DCD et RI de la poignée de main. Les broches, et le fait que le transcepteur inverse ou non les lignes, sont des réglages.
* Les **voyants** : HOME X, HOME Y, HOME Z, ENABLE et TOOL sont lus par un transistor tampon, affichés dans l'état et sur le panneau, et une demande peut faire jeter au nœud le reste d'une recherche dès qu'un axe est en origine, si bien que le programme n'a plus à lire la ligne d'origine au milieu du flux.
* La **broche** : un PWM de vitesse pour un convertisseur 0-10 V (24000 tr/min à signal plein par défaut, pour un variateur de fréquence comme le Huanyang HY02D223B) et des lignes de marche avant / arrière, sans changement de sens pendant qu'elle tourne ni juste après l'arrêt, et un arrêt dès que la machine est arrêtée.
* Le **panneau** (HTTP et HTTPS, huit langues) : mise en service avec un code affiché sur la console USB, utilisateurs, Ethernet ou Wi-Fi, la liaison de commande, les broches RS-232, les voyants et la broche, un journal, et des mises à jour par envoi dans l'un de deux emplacements de micrologiciel avec retour arrière. Rien de secret n'est compilé dans l'image.
* L'**autoprotection** : un garde qui surveille les tâches et la mémoire, une porte devant le panneau quand la mémoire manque, et la confirmation d'une nouvelle image une fois le nœud joignable.
* Les **cartes** : la Waveshare ESP32-S3-ETH (Ethernet W5500) et une ESP32-S3-WROOM-1 N16R8 (Wi-Fi), une image pour chacune. La conception de la carte porteuse (100 x 100 mm) est dans `docs/BOARD.md`.
* Les **machines** : la Bungard CCD MTC et la CCD/2 MTC, avec l'unité de commande sur port série du logiciel DOS 4.6. Les deux modèles ATC ne sont pas couverts ; les XL et les machines postérieures ont leur propre unité de commande moderne.
* La **fin autonome** (désactivée par défaut, un réglage) : quand le programme ou le serveur disparaît, le nœud exécute jusqu'au bout les octets déjà dans sa file - y compris les ordres de broche qui y sont intercalés - puis coupe la broche et les drivers. La file vit dans la RAM externe (jusqu'à 4 Mo, des minutes de mouvement), une limite de temps y met fin, de même qu'un Abort et le bouton d'arrêt du panneau. **Prévu, pas encore fait** : une entrée de bouton pour un arrêt physique.

## 🧩 Rôle dans EH3D

EH3D-NODE-BUNGARD est la dernière étape entre les programmes EH3D et la machine : EH3D-DRILLPRO et EH3D-ROUTEPRO s'y connectent aujourd'hui, et EH3D-SERVER se placera devant. C'est le premier nœud de la famille EH3D et ses réglages, son panneau et son système de mise à jour sont faits pour être partagés avec les nœuds suivants (la base commune vit dans EH3D-COMMON).

## 📂 Structure du dépôt

```text
EH3D-NODE-BUNGARD/
├── core/         les trames et les règles de session de la liaison, la file d'octets et les réglages : sans matériel, testés sur un ordinateur
├── main/         le micrologiciel ESP-IDF : UART, liaison de commande, voyants, broche, réseau, stockage des réglages, serveur web
├── panel/        le panneau web, intégré au micrologiciel
├── tests/        tests d'hôte et vérifications des textes du panneau
├── tools/        build_node.sh, pack_panel.py, check_sdkconfig.py, ci_validate.py, panel_mock.mjs
├── docs/         PROTOCOL, NODE_FIRMWARE, WIRING et BOARD
├── images/       bannière
└── eh3d.project.json
```

## 🛠️ COMPILER ET EXÉCUTER

Il faut un compilateur C++17 et CMake pour les tests d'hôte, et ESP-IDF 5.5 (ou Docker) pour le micrologiciel.

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth` (ou `s3-wifi`) construit une image dans le conteneur ESP-IDF ; sous Windows un ESP-IDF 5.5.5 installé fait la même chose. L'image `dist/eh3d-node-bungard-<version>-<carte>-complete.bin` est pour UNE carte : flashez-la à l'adresse 0 avec `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <image>`. Ensuite, rejoignez le Wi-Fi `EH3D-SETUP-xxxxxx` que le nœud ouvre, ouvrez `http://192.168.4.1/` et saisissez le code de mise en service que le nœud affiche sur sa console USB ; le jeton d'accès, le réseau et les broches se règlent depuis le panneau.

## 🔗 Projets liés

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): le programme de perçage ; il pilote la machine par ce nœud avec `connection = node`
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): le programme de fraisage ; il partage la bibliothèque de protocole et le fichier de la machine
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): les parties communes de la famille EH3D ; la base du micrologiciel du nœud y vit
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): le centre de la famille EH3D : il exécute les travaux des programmes et des autres clients sur la machine par ce nœud

## 📚 Documentation et communauté

* [La liaison de commande](docs/PROTOCOL.md): trames, connexion, armement, file, broche, voyants et règles qui arrêtent la machine.
* [Le micrologiciel](docs/NODE_FIRMWARE.md): mise en service, réglages, panneau, mise à jour avec retour arrière, mémoire et autoprotection.
* [Câblage](docs/WIRING.md): le port RS-232, les voyants et le variateur de la broche.
* [La carte porteuse](docs/BOARD.md): la conception pour dessiner le schéma.
* [Journal des modifications](CHANGELOG.md)
* [Contribuer](CONTRIBUTING.md)
* [Politique de sécurité](SECURITY.md)
* [Assistance](SUPPORT.md)

## 👤 AUTEUR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCE

Logiciel : GPL-3.0-or-later. Voir [LICENSE](LICENSE). Les conceptions matérielles de la suite utilisent la CERN OHL-S.
