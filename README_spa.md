<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center"><a href="README.md">🇺🇸 English</a> | 🇪🇸 <b>Español</b> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Un nodo ESP32-S3 que va en la caja de una máquina Bungard CCD, se conecta al puerto RS-232 de su central y deja que los programas EH3D manejen la máquina por Ethernet o Wi-Fi, con los LED de la máquina y el variador del husillo en la misma placa

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Comprobación de honestidad - lo que se ejecuta hoy:** firmware en C++17 para el ESP32-S3 (ESP-IDF 5.5) que convierte el puerto RS-232 de la central de una máquina Bungard CCD en un puerto de red: un programa se conecta por TCP con un token de acceso, arma la máquina y envía los bytes de un movimiento, que el nodo pone en cola y saca por la línea al ritmo de la propia línea. También lee los LED de la máquina (HOME X, Y y Z, ENABLE, TOOL), maneja un husillo con una señal de velocidad de 0-10 V y líneas de marcha y sentido, y tiene un panel web (ocho idiomas) para la puesta en marcha, los usuarios, la red, la actualización y los ajustes de la máquina. Las reglas del enlace y los ajustes se prueban en un ordenador, y el firmware compila sin avisos para las dos placas. **Todavía no se ha grabado ni ejecutado en una placa**, y la placa portadora que reúne el convertidor RS-232, las entradas de los LED y el variador del husillo es un diseño sobre el papel.

---

> **Estado de seguridad (safety) y de seguridad informática:** esto no es un sistema de seguridad. Un nodo mal cableado, o un variador de husillo mal ajustado, puede mover una máquina o hacer girar una herramienta. El nodo se niega a ser controlado sin un token de acceso, acepta bytes para la máquina solo mientras un programa lo ha armado, y para (cola vaciada, drivers de los motores y husillo apagados) cuando la sesión calla o se cae la conexión (salvo que esté activado el ajuste opcional que le deja terminar el trabajo que tiene). No ve los finales de carrera de la máquina salvo por los LED, y todavía no tiene parada de emergencia propia (la placa portadora deja sitio para una entrada de pulsador). El variador del husillo trabaja a tensión de red: cabléalo y ajústalo con el manual del fabricante. El panel nunca mueve la máquina, y se niega a guardar ajustes mientras un programa tiene la máquina armada. El proyecto es independiente: no lo fabrica, respalda ni da soporte Bungard Elektronik, y los nombres de producto aparecen solo para indicar con qué máquinas y variadores debe funcionar.

## 📋 De un vistazo

| | |
|---|---|
| Versión | `0.0.2` |
| Madurez | Andamiaje |
| Pruebas | 9 suites de pruebas de host (unas 2200 comprobaciones) |
| Pila tecnológica | C++17, ESP-IDF 5.5, ESP32-S3 |
| En hardware real | No: las dos imágenes de firmware compilan, pero nada se ha ejecutado todavía en una placa |

## 🚦 Estado del proyecto

Cada línea es una parte de este repositorio y la luz dice hasta dónde llega hoy. Se escribe con lo que el repositorio puede demostrar (pruebas, compilaciones, ejecuciones), no con lo previsto.

🟢 Hecho y verificado · 🟡 Funciona, con límites conocidos · 🟠 Hecho a medias o sin verificar · 🔴 Sin hacer o sin funcionar

| Área | Estado | Qué significa aquí |
|---|---|---|
| Software | 🟡 | El firmware para el ESP32-S3 (ESP-IDF 5.5, C++17) compila sin avisos para las dos placas, Ethernet y Wi-Fi; no se ha grabado ninguna imagen. |
| Pruebas | 🟢 | Pasan 9 programas de prueba en el PC (`ctest`), unas 2200 comprobaciones: el enlace, los ajustes, las reglas de la máquina, la base compartida, los textos del panel y la seguridad de las tareas. |
| Enlace de control | 🟡 | El protocolo por tramas (CRC, inicio de sesión con token, armado, cola de bytes, parada cuando la sesión calla) está cubierto por las pruebas en el PC; ningún programa ha hablado con un nodo real. |
| Panel web | 🟡 | El panel web (ocho idiomas: puesta en marcha, usuarios, red, ajustes, actualización) tiene sus textos comprobados y se prueba contra una simulación del nodo; nunca se ha servido desde una placa. |
| Husillo y lámparas | 🟠 | La señal de velocidad de 0-10 V, las líneas de marcha y sentido (sin invertir mientras gira) y las cinco entradas de lámparas se prueban en el PC; no se ha conectado ningún variador ni máquina. |
| Actualizaciones | 🟠 | La carga en una de las dos ranuras de firmware con vuelta atrás y confirmación se prueba en el PC; nunca se ha hecho una actualización en una placa. |
| Seguridad | 🟡 | Código de puesta en marcha en la consola USB, token de acceso, usuarios y roles, HTTPS opcional y nada secreto compilado; se detiene solo si se cae el enlace. Todavía no tiene entrada de parada de emergencia ni revisión independiente. |
| Hardware real | 🔴 | Ninguna placa ha ejecutado este firmware, y la placa portadora con el convertidor RS-232, las entradas de lámparas y el variador del husillo es solo un diseño en papel. |
| Documentación | 🟢 | README en ocho idiomas, el protocolo, el firmware, el cableado y el diseño de la placa portadora. |

## 🎯 Descripción general

* El **enlace de control** (TCP, puerto 5800 por defecto): un protocolo con tramas y CRC, un acceso con token, el armado, una cola de bytes para la central y una parada cuando la sesión calla o se cae. El programa envía los bytes de un movimiento ya espaciados y la UART del nodo los saca por la línea con exactitud, algo que un adaptador USB-serie no consigue a velocidad.
* La **línea serie a la central**: 38400 baudios 8N1 a través de un MAX3232, con las líneas DTR, DCD y RI del saludo. Los pines, y si el transceptor invierte las líneas, son ajustes.
* Los **LED**: HOME X, HOME Y, HOME Z, ENABLE y TOOL se leen a través de un transistor separador, se muestran en el estado y en el panel, y una petición puede hacer que el nodo deseche el resto de una búsqueda en cuanto un eje llega a home, así el programa ya no tiene que leer la línea de home en mitad del flujo.
* El **husillo**: un PWM de velocidad para un convertidor de 0-10 V (24000 rpm a señal completa por defecto, para un variador de frecuencia como el Huanyang HY02D223B) y líneas de marcha adelante y atrás, sin cambio de sentido mientras gira ni nada más pararse, y parada siempre que se para la máquina.
* El **panel** (HTTP y HTTPS, ocho idiomas): puesta en marcha con un código que se muestra por la consola USB, usuarios, Ethernet o Wi-Fi, el enlace de control, los pines RS-232, los LED y el husillo, un registro, y actualizaciones por subida a una de dos ranuras de firmware con vuelta atrás. En la imagen no se compila nada secreto.
* La **autoprotección**: un vigilante de las tareas y la memoria, una compuerta a la puerta del panel cuando falta memoria, y la confirmación de una imagen nueva cuando el nodo ya es alcanzable.
* Las **placas**: la Waveshare ESP32-S3-ETH (Ethernet W5500) y una ESP32-S3-WROOM-1 N16R8 (Wi-Fi), una imagen para cada una. El diseño de la placa portadora (100 x 100 mm) está en `docs/BOARD.md`.
* Las **máquinas**: la Bungard CCD MTC y la CCD/2 MTC, con la central por puerto serie del software DOS 4.6. Los dos modelos ATC no están cubiertos; las XL y las máquinas posteriores llevan una central moderna propia.
* El **final autónomo** (desactivado por defecto, es un ajuste): cuando el programa o el servidor desaparece, el nodo ejecuta hasta el final los bytes que ya tiene en la cola - con las órdenes de husillo que van entre ellos - y luego apaga el husillo y los drivers. La cola vive en la RAM externa (hasta 4 MB, minutos de movimiento), un tiempo límite la termina y también un Abort y el botón de paro del panel. **Previsto, aún sin hacer**: una entrada de pulsador para una parada física.

## 🧩 Papel en EH3D

EH3D-NODE-BUNGARD es la última etapa entre los programas EH3D y la máquina: EH3D-DRILLPRO y EH3D-ROUTEPRO se conectan hoy a él, y EH3D-SERVER se pondrá delante. Es el primer nodo de la familia EH3D y sus ajustes, su panel y su sistema de actualización están pensados para compartirse con los nodos que vengan (la base común vive en EH3D-COMMON).

## 📂 Estructura del repositorio

```text
EH3D-NODE-BUNGARD/
├── core/         tramas y reglas de sesión del enlace, la cola de bytes y los ajustes: sin hardware, probados en un ordenador
├── main/         el firmware ESP-IDF: UART, enlace de control, LED, husillo, red, almacén de ajustes, servidor web
├── panel/        el panel web, empaquetado en el firmware
├── tests/        pruebas de host y comprobaciones de los textos del panel
├── tools/        build_node.sh, pack_panel.py, check_sdkconfig.py, ci_validate.py, panel_mock.mjs
├── CMakeLists.txt, partitions.csv, sdkconfig.defaults, sdkconfig.board.*  el proyecto ESP-IDF, la distribución de la flash y los ajustes de cada placa (s3-eth, s3-wifi)
├── docs/         PROTOCOL, NODE_FIRMWARE, WIRING y BOARD
├── images/       banner
└── eh3d.project.json
```

## 🛠️ COMPILAR Y EJECUTAR

Necesitas un compilador C++17 y CMake para las pruebas de host, y ESP-IDF 5.5 (o Docker) para el firmware.

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth` (o `s3-wifi`) construye una imagen en el contenedor de ESP-IDF; en Windows un ESP-IDF 5.5.5 instalado hace lo mismo. La imagen `dist/eh3d-node-bungard-<versión>-<placa>-complete.bin` es para UNA placa: grábala en la dirección 0 con `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <imagen>`. Después conéctate a la Wi-Fi `EH3D-SETUP-xxxxxx` que abre el nodo, abre `http://192.168.4.1/` e introduce el código de puesta en marcha que el nodo muestra por su consola USB; el token de acceso, la red y los pines se ponen desde el panel.

## 🔗 Proyectos relacionados

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): el programa de taladrado; maneja la máquina a través de este nodo con `connection = node`
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): el programa de fresado; comparte la biblioteca de protocolo y el fichero de la máquina
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): las partes comunes de la familia EH3D; la base del firmware del nodo vive allí
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): el centro de la familia EH3D: ejecuta los trabajos de los programas y de los demás clientes en la máquina a través de este nodo

## 📚 Documentación y comunidad

* [El enlace de control](docs/PROTOCOL.md): tramas, acceso, armado, cola, husillo, LED y las reglas que paran la máquina.
* [El firmware](docs/NODE_FIRMWARE.md): puesta en marcha, ajustes, panel, actualización con vuelta atrás, memoria y autoprotección.
* [Cableado](docs/WIRING.md): el puerto RS-232, los LED y el variador del husillo.
* [La placa portadora](docs/BOARD.md): el diseño para dibujar el esquema.
* [Registro de cambios](CHANGELOG.md)
* [Cómo contribuir](CONTRIBUTING.md)
* [Política de seguridad](SECURITY.md)
* [Soporte](SUPPORT.md)

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCIA

Software: GPL-3.0-or-later. Véase [LICENSE](LICENSE). Los diseños de hardware de la suite usan CERN OHL-S.
