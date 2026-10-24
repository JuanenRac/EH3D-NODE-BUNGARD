<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | 🇵🇹 <b>Português</b></p>

### Um nó ESP32-S3 que fica na caixa de uma máquina Bungard CCD, liga-se à porta RS-232 da sua unidade de controlo e deixa os programas EH3D conduzir a máquina por Ethernet ou Wi-Fi, com os LED da máquina e o variador do eixo-árvore na mesma placa

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Verificação de honestidade - o que funciona hoje:** firmware em C++17 para o ESP32-S3 (ESP-IDF 5.5) que transforma a porta RS-232 da unidade de controlo de uma máquina Bungard CCD numa porta de rede: um programa liga-se por TCP com um token de acesso, arma a máquina e envia os bytes de um movimento, que o nó põe em fila e envia pela linha ao ritmo da própria linha. Lê também os LED da máquina (HOME X, Y e Z, ENABLE, TOOL), comanda um eixo-árvore com um sinal de velocidade de 0-10 V e linhas de marcha e sentido, e tem um painel web (oito idiomas) para a instalação, os utilizadores, a rede, a atualização e as definições próprias da máquina. As regras da ligação e as definições são testadas num computador, e o firmware compila sem avisos para as duas placas. **Ainda não foi gravado nem executado numa placa**, e a placa portadora que reúne o conversor RS-232, as entradas dos LED e o variador do eixo-árvore é um desenho em papel.

---

> **Estado de segurança (safety) e de segurança informática:** isto não é um sistema de segurança. Um nó mal ligado, ou um variador de eixo-árvore mal configurado, pode mover uma máquina ou fazer girar uma ferramenta. O nó recusa ser controlado sem um token de acesso, aceita bytes para a máquina só enquanto um programa o armou, e pára (fila esvaziada, drivers dos motores e eixo-árvore desligados) quando a sessão se cala ou a ligação cai (a não ser que esteja ligada a definição opcional que o deixa terminar o trabalho que tem). Só vê os fins de curso da máquina através dos LED, e ainda não tem paragem de emergência própria (a placa portadora prevê uma entrada para um botão). O variador do eixo-árvore está à tensão da rede: ligue-o e configure-o com o manual do fabricante. O painel nunca move a máquina, e recusa guardar definições enquanto um programa tem a máquina armada. O projeto é independente: não é fabricado, apoiado nem suportado pela Bungard Elektronik, e os nomes de produtos aparecem apenas para dizer com que máquinas e variadores deve funcionar.

## 📋 Num relance

| | |
|---|---|
| Versão | `0.0.2` |
| Maturidade | Andaime |
| Testes | 9 suites de testes de anfitrião (cerca de 2200 verificações) |
| Tecnologias | C++17, ESP-IDF 5.5, ESP32-S3 |
| Em hardware real | Não: as duas imagens de firmware compilam, mas ainda nada correu numa placa |

## 🎯 Descrição geral

* A **ligação de controlo** (TCP, porta 5800 por omissão): um protocolo de tramas com CRC, uma entrada com token de acesso, o armar, uma fila de bytes para a unidade de controlo e uma paragem quando a sessão se cala ou cai. O programa envia os bytes de um movimento já espaçados e a UART do nó coloca-os na linha com exatidão, o que um adaptador USB-série não consegue a velocidade.
* A **linha série para a unidade de controlo**: 38400 baud 8N1 através de um MAX3232, com as linhas DTR, DCD e RI do cumprimento. Os pinos, e se o transcetor inverte as linhas, são definições.
* Os **LED**: HOME X, HOME Y, HOME Z, ENABLE e TOOL são lidos através de um transístor separador, mostrados no estado e no painel, e um pedido pode fazer o nó descartar o resto de uma procura assim que um eixo chega a casa, de modo que o programa já não tem de ler a linha de home a meio do fluxo.
* O **eixo-árvore**: um PWM de velocidade para um conversor de 0-10 V (24000 rpm a sinal completo por omissão, para um variador de frequência como o Huanyang HY02D223B) e linhas de marcha em frente / atrás, sem mudança de sentido enquanto gira nem logo após parar, e paragem sempre que a máquina é parada.
* O **painel** (HTTP e HTTPS, oito idiomas): instalação com um código mostrado na consola USB, utilizadores, Ethernet ou Wi-Fi, a ligação de controlo, os pinos RS-232, os LED e o eixo-árvore, um registo, e atualizações por envio para uma de duas ranhuras de firmware com reversão. Nada de secreto é compilado na imagem.
* A **autoproteção**: um vigia das tarefas e da memória, um portão à porta do painel quando falta memória, e a confirmação de uma imagem nova quando o nó já é alcançável.
* As **placas**: a Waveshare ESP32-S3-ETH (Ethernet W5500) e uma ESP32-S3-WROOM-1 N16R8 (Wi-Fi), uma imagem para cada. O desenho da placa portadora (100 x 100 mm) está em `docs/BOARD.md`.
* As **máquinas**: a Bungard CCD MTC e a CCD/2 MTC, com a unidade de controlo por porta série do software DOS 4.6. Os dois modelos ATC não estão abrangidos; as XL e as máquinas posteriores têm uma unidade de controlo moderna própria.
* O **final autónomo** (desligado por omissão, é uma definição): quando o programa ou o servidor desaparece, o nó executa até ao fim os bytes que já tem na fila - incluindo as ordens de eixo-árvore intercaladas - e depois desliga o eixo-árvore e os drivers. A fila vive na RAM externa (até 4 MB, minutos de movimento); um limite de tempo termina-o, tal como um Abort e o botão de paragem do painel. **Previsto, ainda por fazer**: uma entrada de botão para uma paragem física.

## 🧩 Papel no EH3D

O EH3D-NODE-BUNGARD é a última etapa entre os programas EH3D e a máquina: o EH3D-DRILLPRO e o EH3D-ROUTEPRO ligam-se hoje a ele, e o EH3D-SERVER ficará à frente. É o primeiro nó da família EH3D e as suas definições, o seu painel e o sistema de atualização foram pensados para ser partilhados com os nós que vierem (a base comum vive no EH3D-COMMON).

## 📂 Estrutura do repositório

```text
EH3D-NODE-BUNGARD/
├── core/         as tramas e as regras de sessão da ligação, a fila de bytes e as definições: sem hardware, testados num computador
├── main/         o firmware ESP-IDF: UART, ligação de controlo, LED, eixo-árvore, rede, armazém de definições, servidor web
├── panel/        o painel web, empacotado no firmware
├── tests/        testes de anfitrião e verificações dos textos do painel
├── tools/        build_node.sh, pack_panel.py, check_sdkconfig.py, ci_validate.py, panel_mock.mjs
├── docs/         PROTOCOL, NODE_FIRMWARE, WIRING e BOARD
├── images/       banner
└── eh3d.project.json
```

## 🛠️ COMPILAR E EXECUTAR

Precisa de um compilador C++17 e CMake para os testes de anfitrião, e do ESP-IDF 5.5 (ou Docker) para o firmware.

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth` (ou `s3-wifi`) constrói uma imagem no contentor ESP-IDF; no Windows um ESP-IDF 5.5.5 instalado faz o mesmo. A imagem `dist/eh3d-node-bungard-<versão>-<placa>-complete.bin` é para UMA placa: grave-a no endereço 0 com `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <imagem>`. Depois ligue-se ao Wi-Fi `EH3D-SETUP-xxxxxx` que o nó abre, abra `http://192.168.4.1/` e introduza o código de instalação que o nó mostra na sua consola USB; o token de acesso, a rede e os pinos definem-se no painel.

## 🔗 Projetos relacionados

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): o programa de furação; conduz a máquina através deste nó com `connection = node`
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): o programa de fresagem; partilha a biblioteca de protocolo e o ficheiro da máquina
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): as partes comuns da família EH3D; a base do firmware do nó vive lá
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): o centro da família EH3D: executa os trabalhos dos programas e dos outros clientes na máquina através deste nó

## 📚 Documentação e comunidade

* [A ligação de controlo](docs/PROTOCOL.md): tramas, entrada, armar, fila, eixo-árvore, LED e as regras que param a máquina.
* [O firmware](docs/NODE_FIRMWARE.md): instalação, definições, painel, atualização com reversão, memória e autoproteção.
* [Cablagem](docs/WIRING.md): a porta RS-232, os LED e o variador do eixo-árvore.
* [A placa portadora](docs/BOARD.md): o desenho para fazer o esquema.
* [Registo de alterações](CHANGELOG.md)
* [Como contribuir](CONTRIBUTING.md)
* [Política de segurança](SECURITY.md)
* [Suporte](SUPPORT.md)

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENÇA

Software: GPL-3.0-or-later. Ver [LICENSE](LICENSE). Os projetos de hardware da suite usam a CERN OHL-S.
