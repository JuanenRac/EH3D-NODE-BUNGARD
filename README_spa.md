<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center"><a href="README.md">🇺🇸 English</a> | 🇪🇸 <b>Español</b> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Un nodo ESP32-S3 que va en la caja de una máquina Bungard CCD, se conecta al puerto RS-232 de su unidad de control y deja que los programas EH3D manejen la máquina por Ethernet o Wi-Fi, con los LED de la máquina y el variador del husillo en la misma placa

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.4-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Comprobación de honestidad - lo que se ejecuta hoy:** firmware en C++17 para el ESP32-S3 (ESP-IDF 5.5) que convierte el puerto RS-232 de la unidad de control de una máquina Bungard CCD en un puerto de red: un programa se conecta por TCP con un token de acceso, arma la máquina y envía los bytes de un movimiento, que el nodo pone en cola y saca a la línea al ritmo propio de la línea. También lee los LED de la máquina (HOME X, Y y Z, ENABLE, TOOL), maneja un husillo con una señal de velocidad de 0-10 V y líneas de marcha y sentido, y tiene un panel web con menú lateral (ocho idiomas) para la puesta en marcha, los usuarios, la red, la actualización y los ajustes propios de la máquina. Las reglas del enlace y los ajustes se prueban en un ordenador (10 programas de prueba, 2235 comprobaciones contadas) y el firmware compila sin avisos para las dos placas. **Todavía no se ha grabado ni ejecutado en una placa**, no se ha conectado nada a una máquina, y la placa portadora que reúne el convertidor RS-232, las entradas de los LED y el variador del husillo es un diseño sobre el papel.

---

> **Estado de seguridad (safety) y de seguridad informática:** esto no es un sistema de seguridad (safety). Un nodo mal cableado, o un variador de husillo mal ajustado, puede mover una máquina o hacer girar una herramienta. El nodo se niega a ser controlado sin un token de acceso, acepta bytes para la máquina solo mientras un programa la ha armado y se detiene (cola vaciada, drivers de motores y husillo apagados) cuando la sesión calla o la conexión se cae (salvo que esté activado el ajuste opcional que le deja terminar el trabajo que tiene). No ve los finales de carrera de la máquina salvo a través de los LED y todavía no tiene parada de emergencia propia (la placa portadora tiene sitio para una entrada de pulsador). El variador del husillo está a tensión de red: cabléalo y ajústalo con el manual del fabricante. El panel nunca mueve la máquina y se niega a guardar ajustes mientras un programa tiene la máquina armada. El proyecto es independiente: no está hecho, avalado ni respaldado por Bungard Elektronik, y los nombres de producto aparecen solo para decir con qué máquinas y variadores está pensado para funcionar.

## 📋 De un vistazo

| | |
|---|---|
| Versión | `0.0.4` |
| Madurez | Andamiaje |
| Pruebas | 10 programas de prueba en el ordenador (2235 comprobaciones contadas) |
| Pila tecnológica | C++17, ESP-IDF 5.5, ESP32-S3 |
| En hardware real | No: las dos imágenes de firmware compilan, pero nada se ha grabado ni ejecutado en una placa y nada se ha conectado a una máquina |

**Ir a:** [Resumen](#-resumen) · [Arquitectura](#-arquitectura) · [Inicio rápido](#-inicio-rápido) · [Pruebas](#-pruebas) · [Estructura](#-estructura-del-repositorio) · [Relacionados](#-proyectos-relacionados)

## 🚦 Estado del proyecto

Cada línea es una parte de este repositorio y la luz dice hasta dónde llega hoy. Se escribe con lo que el repositorio puede demostrar (pruebas, compilaciones, ejecuciones), no con lo previsto.

🟢 Hecho y verificado · 🟡 Funciona, con límites conocidos · 🟠 Hecho a medias o sin verificar · 🔴 Sin hacer o sin funcionar

| Área | Estado | Qué significa aquí |
|---|---|---|
| Software | 🟡 | El firmware para el ESP32-S3 (ESP-IDF 5.5, C++17) compila sin avisos para las dos placas, Ethernet y Wi-Fi; no se ha grabado ninguna imagen. |
| Pruebas | 🟡 | Pasan 10 programas de prueba en el ordenador (`ctest`): 2235 comprobaciones contadas del enlace, los ajustes, las reglas de la máquina y la base compartida, más la comprobación de los textos del panel (315 filas en 8 idiomas) y de las pilas de las tareas (10 tareas). |
| Enlace de control | 🟡 | El protocolo con tramas (CRC, inicio de sesión con token, armado, cola de bytes, parada cuando la sesión calla) está cubierto por las pruebas en el ordenador; ningún programa ha hablado con un nodo real. |
| Panel web | 🟡 | El panel web (menú lateral, ocho idiomas: puesta en marcha, máquina, red, usuarios, actualización, registro, sistema) tiene sus textos comprobados y se ejercita contra una simulación del nodo en un navegador; nunca se ha servido desde una placa. |
| Husillo y LED | 🟠 | La señal de velocidad de 0-10 V, las líneas de marcha y sentido (sin invertir mientras gira) y las cinco entradas de LED se prueban en el ordenador; no se ha conectado ningún variador ni máquina. |
| Actualizaciones | 🟠 | La subida a una de las dos ranuras de firmware con vuelta atrás y confirmación se prueba en el ordenador; una actualización nunca se ha ejecutado en una placa. |
| Seguridad | 🟡 | Código de puesta en marcha en la consola USB, token de acceso, usuarios y roles, HTTPS opcional y nada secreto compilado; se detiene solo cuando el enlace se cae. Todavía no tiene entrada de parada de emergencia ni revisión independiente. |
| Hardware real | 🔴 | Ninguna placa ha ejecutado este firmware, y la placa portadora con el convertidor RS-232, las entradas de LED y el variador del husillo es solo un diseño sobre el papel. |
| Documentación | 🟡 | README en ocho idiomas, el protocolo, el firmware, el cableado y el diseño de la placa portadora. |

## 🎯 Resumen

* El **enlace de control** (TCP, puerto 5800 por defecto): un protocolo con tramas y CRC, un inicio de sesión con token de acceso, el armado, una cola de bytes para la unidad de control y una parada cuando la sesión calla o se cae. El programa envía los bytes de un movimiento ya espaciados y la UART del nodo los pone en la línea con exactitud, algo que un adaptador USB-serie no puede hacer a velocidad.
* La **línea serie hacia la unidad de control**: 38400 baudios 8N1 a través de un MAX3232, con las líneas DTR, DCD y RI del saludo. Los pines y si el transceptor invierte las líneas son ajustes.
* Los **LED**: HOME X, HOME Y, HOME Z, ENABLE y TOOL se leen a través de un transistor separador, se muestran en el estado y en el panel, y una petición puede hacer que el nodo abandone el resto de una búsqueda en cuanto un eje llega a casa, así que el programa ya no tiene que leer la línea de casa en mitad del flujo.
* El **husillo**: un PWM de velocidad para un convertidor de 0-10 V (24000 rpm a señal máxima por defecto, para un variador de frecuencia como el Huanyang HY02D223B) y líneas de marcha adelante / atrás, sin cambio de sentido mientras gira ni justo después de pararse, y una parada siempre que se detiene la máquina.
* El **panel** (HTTP y HTTPS, ocho idiomas, el menú a la izquierda en una pantalla ancha y arriba en un teléfono): puesta en marcha con un código mostrado en la consola USB, usuarios, Ethernet o Wi-Fi, el enlace de control, los pines RS-232, los LED y el husillo, un registro y actualizaciones por subida a una de las dos ranuras de firmware con vuelta atrás. No se compila nada secreto en la imagen.
* La **autoprotección**: un vigilante que observa las tareas y la memoria, una compuerta a la puerta del panel cuando falta memoria y la confirmación de una imagen nueva en cuanto se puede llegar al nodo.
* Las **placas**: la Waveshare ESP32-S3-ETH (Ethernet W5500) y una ESP32-S3-WROOM-1 N16R8 (Wi-Fi), una imagen para cada una. El diseño de la placa portadora (100 x 100 mm) está en `docs/BOARD.md`.
* Las **máquinas**: la Bungard CCD MTC y la CCD/2 MTC, con la unidad de control de puerto serie del software DOS 4.6. Los dos modelos ATC no están cubiertos; los modelos XL y las máquinas posteriores tienen una unidad de control moderna propia.
* El **final autónomo** (desactivado por defecto, un ajuste): cuando el programa o el servidor desaparece, el nodo ejecuta hasta el final los bytes que ya tiene en la cola - incluidas las órdenes de husillo en cola - y luego apaga el husillo y los drivers. La cola vive en la RAM externa (hasta 4 MB, minutos de movimiento), un límite de tiempo la termina y también un Abort y el botón de parada del panel. **Previsto, aún sin construir**: una entrada de pulsador para una parada física.

## 🧩 Papel en EH3D

EH3D-NODE-BUNGARD es la última etapa entre los programas EH3D y la máquina: EH3D-DRILLPRO y EH3D-ROUTEPRO se conectan a él hoy, y EH3D-SERVER se coloca delante para los demás clientes. Es el primer nodo de la familia EH3D y sus ajustes, su panel y su mecanismo de actualización están pensados para compartirse con los nodos siguientes (la base común vive en EH3D-COMMON).

## 🏛️ Arquitectura

<p align="center">
  <img src="images/architecture.svg" alt="Arquitectura de EH3D-NODE-BUNGARD: quién lo usa, qué contiene y de qué depende" width="100%">
</p>

Cómo encajan las partes de este repositorio: quién lo usa, qué contiene y de qué depende. La imagen se dibuja desde `docs/architecture.json` y se comprueba contra el código.

## 🖼️ Capturas de pantalla

<p align="center">
  <img src="images/screenshots/panel-overview.png" alt="panel-overview" width="100%">
</p>

<p align="center"><em>La página Resumen: el nodo, su hardware, su reloj, su flash con los dos slots de firmware y su red.</em></p>

<p align="center">
  <img src="images/screenshots/panel-status.png" alt="panel-status" width="100%">
</p>

<p align="center"><em>La página Estado, en el tema Forge de los otros programas EH3D.</em></p>

<p align="center">
  <img src="images/screenshots/panel-machine.png" alt="panel-machine" width="100%">
</p>

<p align="center"><em>La página Máquina: el enlace de control, los pines RS-232, las lámparas, el husillo y el token de acceso.</em></p>

<p align="center">
  <img src="images/screenshots/panel-wifi.png" alt="panel-wifi" width="100%">
</p>

<p align="center"><em>La página Wi-Fi: la red a la que se une el nodo y hasta tres redes de respaldo por si se pierde.</em></p>

<p align="center">
  <img src="images/screenshots/panel-update.png" alt="panel-update" width="100%">
</p>

<p align="center"><em>La página Firmware: la actualización desde GitHub, la subida de un fichero y los dos slots, con la copia de uno sobre otro.</em></p>

<p align="center">
  <img src="images/screenshots/panel-help.png" alt="panel-help" width="100%">
</p>

<p align="center"><em>La página Ayuda: un tema para cada página del panel.</em></p>

## ✨ Funciones

* **Un panel con menú lateral:** siete páginas (Estado, Máquina, Red, Usuarios, Firmware, Registro, Sistema), el menú a la izquierda en pantalla ancha y arriba en un teléfono, con el idioma, el tema y el cierre de sesión a su lado.
* **Una imagen, sin secretos:** todo lo que cambia de un nodo a otro es un documento de ajustes en la flash editado desde el panel; no se compila ninguna contraseña, token ni red.
* **Bytes con la temporización correcta:** el programa coloca cada paso y la UART del nodo envía los bytes al ritmo propio de la línea, algo que un adaptador USB no puede mantener.
* **Casa vista en vivo:** se leen los cinco LED y una petición abandona el resto de una búsqueda en cuanto un eje llega a casa.
* **Husillo con barandillas:** la velocidad se limita a la máxima para la que se construyó el nodo, no invierte mientras gira ni justo después de pararse, y se detiene con la máquina.
* **Actualización con vuelta atrás:** una imagen entra en la ranura que no está en uso y queda a prueba hasta que se puede llegar al nodo; el cargador de arranque vuelve atrás cuando no se puede.
* **Se cuida solo:** un vigilante observa las tareas y la memoria, y un nodo que no tiene dirección tras noventa segundos abre de nuevo la red de puesta en marcha para no quedar inalcanzable.
* **El panel del nodo como lo tienen los nodos de A.R.M.O.R.:** un menú en grupos (nodo, red, sistema) con el idioma y el cierre de sesión al pie; un Resumen (el nodo, su hardware, su reloj, su flash y su red), una página Wi-Fi con hasta tres redes de respaldo que el nodo prueba cuando se pierde la principal, una Ayuda completa y una página Acerca de y, en Firmware, la actualización desde GitHub (busca la última versión del proyecto, comprueba su SHA-256 y la instala en la otra ranura), el cambio entre las dos ranuras y la copia de la que corre en la otra.

## 🔒 Seguridad y protección

| Amenaza | Defensa |
|---|---|
| Alguien en la red manejando la máquina | el enlace rechaza todo salvo Hello hasta que el token de acceso (de 8 a 64 caracteres imprimibles) sea correcto; un nodo sin token rechaza el control salvo que esté activado el ajuste de banco `allow_open` (desactivado por defecto); un programa cada vez |
| Un programa que mueve la máquina por accidente | los bytes se aceptan solo con el nodo armado y solo si la trama entera cabe en la cola; una orden de husillo se recorta a la velocidad máxima y se rechaza si invertiría un husillo que gira |
| Un cable que se suelta o un programa que muere a mitad de un movimiento | una sesión armada que calla durante `idle_ms` (3000 ms por defecto) o cuya conexión se cae vacía la cola y envía el byte de drivers apagados `0xC0`; el husillo pasa a 0 rpm |
| Alguien que llega al panel | el primer administrador se crea con un código de puesta en marcha que solo aparece en la consola USB; dos roles (administrador, observador); una contraseña errónea se ralentiza y una dirección de origen que sigue fallando se bloquea un tiempo; HTTP, HTTPS o ambos |
| Secretos en el firmware o en el repositorio | no se compila nada secreto: el token, la red y los pines son ajustes en flash; el token es de solo escritura en el panel y en la API |
| Una escritura de ajustes que detiene un movimiento | mientras un programa tiene la máquina armada el panel se niega a guardar ajustes (HTTP 409 `machine_armed`) y aplaza un reinicio |
| Una asignación de pines errónea | se rechazan los pines que la placa reserva (flash, PSRAM, USB, el chip Ethernet) y ningún pin puede hacer dos trabajos |
| Una imagen de firmware mala | se rechaza una imagen de otro proyecto o una ranura vacía; una imagen nueva está a prueba y el cargador de arranque vuelve a la anterior si el panel no arranca o no se puede llegar al nodo en cinco minutos |
| Un nodo que se queda sin memoria o se cuelga | un vigilante observa las tareas y la memoria interna y reinicia sin crear un bucle; una compuerta a la puerta del panel cuando falta memoria |
| Alguien con el nodo en las manos | mantener BOOT ocho segundos en los primeros treinta tras el encendido borra los ajustes y los usuarios (el acceso físico es un reinicio por diseño) |

**Lo que no se defiende:** el nodo no tiene todavía entrada de parada de emergencia y no ve los finales de carrera salvo por los LED; la actualización no está firmada (la actualización desde GitHub comprueba el SHA-256 que publica la versión, por HTTPS, y solo cuando un administrador lo pide; aún falta una clave de firma); el certificado propio del panel HTTPS hace que el navegador avise; nada se ha ejecutado en una placa y no ha habido revisión independiente. Informa de una vulnerabilidad en privado (véase [SECURITY.md](SECURITY.md)).

## 🔌 Interfaces

| Interfaz | Qué es | Dónde |
|---|---|---|
| Enlace de control | TCP, puerto 5800 por defecto: tramas `0xB5, tipo, longitud, carga, CRC-16`, Hello con el token, Arm, Write, Abort, Ping, StopOnHome, Spindle; el nodo responde Info, Status (cada 200 ms), Rx, HomeSeen y Error | [docs/PROTOCOL.md](docs/PROTOCOL.md), `core/frame.hpp`, `main/link_server.cpp` |
| Panel web | HTTP en el puerto 80 y HTTPS en el 443 (el modo es un ajuste), ocho idiomas, API bajo `/api/v1/` (`GET`/`PUT /api/v1/machine`, `GET /api/v1/machine/status` y las rutas compartidas de la base) | [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md), `main/web_server.cpp`, `panel/` |
| RS-232 hacia la unidad de control | UART a 38400 baudios 8N1 a través de un MAX3232, con DTR (salida), DCD y RI (entradas) | [docs/WIRING.md](docs/WIRING.md), `main/central_uart.cpp` |
| LED | HOME X, HOME Y, HOME Z, ENABLE y TOOL, cinco entradas a través de un transistor separador | [docs/WIRING.md](docs/WIRING.md), `main/lamps.cpp` |
| Husillo | un PWM de velocidad para un convertidor de 0-10 V y líneas de marcha adelante / atrás | [docs/BOARD.md](docs/BOARD.md), `main/spindle.cpp` |
| Consola USB | muestra el código de puesta en marcha y admite Improv Wi-Fi por serie | `main/improv_serial.cpp` |
| Red de puesta en marcha | el nodo abre el Wi-Fi `EH3D-SETUP-xxxxxx` mientras no tiene usuarios | [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md) |

## ⚙️ Configuración

Todo es un ajuste en la flash, editado desde el panel (página Máquina), así que una misma imagen sirve para cada nodo. Los valores de abajo son los de un nodo que nunca se ha configurado; un valor fuera de rango se rechaza al guardarlo.

| Ajuste | Por defecto | Rango | Qué hace |
|---|---|---|---|
| `machine.link.port` | 5800 | de 1024 a 65535 | el puerto TCP del enlace de control |
| `machine.link.token` | ninguno | de 8 a 64 caracteres imprimibles; solo escritura | el token de acceso; sin uno el nodo se niega a ser controlado |
| `machine.link.allow_open` | desactivado | activado o desactivado | un ajuste de banco: acepta control sin token |
| `machine.link.idle_ms` | 3000 | de 200 a 60000 | cuánto puede callar una sesión armada antes de que el nodo se detenga |
| `machine.link.queue_bytes` | 262144 | de 1024 a 4194304 | la cola de bytes para la unidad de control, en la RAM externa |
| `machine.link.finish_on_loss` | desactivado | activado o desactivado | terminar el trabajo que ya está en la cola cuando el programa desaparece |
| `machine.link.max_drain_s` | 600 | de 10 a 3600 | los segundos máximos que el nodo puede seguir solo en ese caso |
| `machine.uart.*` | tx 17, rx 18, dtr 15, dcd 16, ri 21, cts y dsr sin cablear; el transceptor invierte | un pin libre de la placa, o -1 donde es opcional | los pines de la línea serie |
| `machine.lamps.*` | HOME X 1, Y 2, Z 7, ENABLE 8, TOOL 38; activo a nivel bajo | un pin libre | las cinco entradas de LED |
| `machine.spindle.max_rpm` | 24000 | de 0 a 65535 (0: sin salida de husillo) | la velocidad a señal máxima |
| `machine.spindle.pwm_hz` | 2000 | de 100 a 40000 | la frecuencia de la señal de velocidad |
| `machine.spindle.reverse_delay_ms` | 3000 | de 0 a 60000 | cuánto tiempo debe llevar parado el husillo antes de poder girar al otro lado |
| `web.mode` | `both` | `http`, `both` o `https` | si el panel se sirve en el puerto 80, en el 443 o en ambos |

Los pines del husillo (PWM 4, adelante 5, atrás 6) también son ajustes. Los ajustes que tiene todo nodo (nombre, vía de acceso, punto de acceso, dirección, reloj, idioma, reinicio periódico) están en la base y se describen en [docs/NODE_FIRMWARE.md](docs/NODE_FIRMWARE.md).

## 🚀 Inicio rápido

Necesitas un compilador de C++17 y CMake para las pruebas en el ordenador, y ESP-IDF 5.5 (o Docker) para el firmware.

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth` (o `s3-wifi`) compila una imagen en el contenedor de ESP-IDF; en Windows un ESP-IDF 5.5.5 instalado hace lo mismo. La imagen `dist/eh3d-node-bungard-<versión>-<placa>-complete.bin` es para UNA placa: grábala en la dirección 0 con `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <imagen>`. Después únete al Wi-Fi `EH3D-SETUP-xxxxxx` que abre el nodo, abre `http://192.168.4.1/` e introduce el código de puesta en marcha que el nodo muestra en su consola USB; el token de acceso, la red y los pines se ajustan desde el panel. Este camino todavía no se ha seguido en una placa real.

## 🧪 Pruebas

Las pruebas en el ordenador (`build.bat` o `./build.sh`, luego `ctest`) son 10 programas y todos pasan: `core_tests` (104 comprobaciones: las tramas y las reglas de sesión), `test_base` (382), `test_clock` (imprime ok), `test_settings` (392), `test_machine` (75), `test_shared` (627) y `test_shared_eth` (629, el perfil de la placa Ethernet) - 2235 comprobaciones contadas - más `panel_text` (315 filas en 8 idiomas, 265 claves usadas, 36 códigos de error del firmware con texto) y `task_safety` (10 tareas, ninguna copia grande de los ajustes en una pila pequeña). Cubren el CRC y el decodificador que se resincroniza tras un daño, el inicio de sesión con token, el armado, la parada por silencio, las reglas del husillo (tope, sin inversión, retardo), los ajustes y sus rangos, conflictos de pines y pines reservados, usuarios y sesiones, el plan de red, el reloj y JSON. El panel también se ha ejercitado en un navegador contra `tools/panel_mock.mjs`, un nodo de pega que lo sirve sin placa. El CI ejecuta `python -I tools/ci_validate.py` y las pruebas en el ordenador en Linux con las advertencias como errores. La compilación de las dos imágenes de firmware (ESP-IDF 5.5.5) se hace a mano y no forma parte del CI; nada se ejecuta en una placa.

## 🏗️ Compilación y publicación

La versión vive en `eh3d.project.json` y en la compilación, y cada versión tiene su entrada en [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` compilan y ejecutan las pruebas en el ordenador y `test.sh` / `test.bat` ejecutan solo las pruebas, cada uno con un panel en vivo; ninguno cambia la versión. Las imágenes de firmware salen de `tools/build_node.sh` (o de ESP-IDF 5.5.5 en Windows) y se llaman `eh3d-node-bungard-<versión>-<placa>-{app,complete}.bin`. Todavía no se ha publicado ninguna versión ni se ha grabado ninguna imagen. La actualización desde GitHub está construida y nunca se ha ejecutado contra el GitHub real en una placa; las versiones no están firmadas, así que confía en el hash que publican.

## 🩺 Resolución de problemas

| Síntoma | Qué hacer |
|---|---|
| No se llega al nodo | Un nodo que no tiene dirección tras noventa segundos abre de nuevo la red de puesta en marcha `EH3D-SETUP-xxxxxx`; únete a ella y ajusta la vía de acceso desde el panel. |
| Se ha perdido el código de puesta en marcha | Solo aparece en la consola USB, cada quince segundos. Mantener BOOT ocho segundos en los primeros treinta tras el encendido borra los ajustes y los usuarios y empieza de nuevo la puesta en marcha. |
| El enlace responde Error 1 o 2 | No se ha iniciado sesión o el token es erróneo: la primera trama debe ser Hello con el token de acceso fijado en el panel (página Máquina). |
| Error 3 o 4 | No está armado (envía Arm primero) o la cola está llena (espera a que el Status muestre hueco). |
| Error 7 | La sesión armada calló durante `idle_ms`: envía Ping mientras haya un movimiento largo en cola. |
| Error 9, 10 u 11 | El husillo debe pararse antes de invertir, llevaba parado muy poco para invertir, o el nodo está terminando el trabajo que tiene. |
| El panel se niega a guardar (409 `machine_armed`) | Un programa tiene la máquina armada; desármala o pulsa el botón de parada de la página Estado, y guarda de nuevo. |
| El navegador avisa del panel HTTPS | El nodo crea su propio certificado la primera vez; usa el modo `http` o `both` en una red de confianza, o acepta el certificado. |

## 📂 Estructura del repositorio

<p align="center">
  <img src="images/repository-structure.svg" alt="Estructura del repositorio EH3D-NODE-BUNGARD" width="100%">
</p>

<details>
<summary>Versión en texto del árbol</summary>

```text
EH3D-NODE-BUNGARD/
├── core/
│   ├── frame.hpp session.hpp                    tramas y reglas de sesión
│   ├── byte_queue.hpp drain.hpp                 cola de bytes, final del trabajo
│   ├── bungard_config.hpp                       ajustes de máquina y pines
│   ├── node_settings.hpp auth.hpp               ajustes y usuarios (base común)
│   ├── netplan.hpp json.hpp                     plan de red y JSON (base común)
│   └── release_assets.hpp semver.hpp            qué fichero de una versión es la imagen de esta placa
├── main/
│   ├── app_main.cpp link_server.cpp             arranque y enlace de control
│   ├── central_uart.cpp lamps.cpp               línea serie y LED
│   ├── spindle.cpp machine_api.cpp              salidas de husillo, API
│   ├── web_server.cpp network.cpp               servidor del panel, red (base)
│   ├── node_store.cpp node_guard.cpp            almacén y vigilante (base)
│   └── github_update.cpp entropy.cpp            la actualización desde GitHub y la copia de una ranura
├── panel/
│   ├── index.html style.css                     la página y la disposición del menú lateral
│   └── app.js text.js                           las páginas y los textos en ocho idiomas
├── tests/                                       pruebas en el ordenador del enlace, los ajustes, la base compartida y los textos del panel
├── docs/                                        protocolo, firmware, cableado, placa
├── tools/                                       compilación, panel, comprobaciones, nodo de pega
├── CMakeLists.txt                               el proyecto ESP-IDF
├── partitions.csv                               la flash con dos ranuras
├── sdkconfig.*                                  ajustes ESP-IDF y perfiles de placa
├── images/                                      banner y las imágenes de Arquitectura y Estructura del repositorio
├── .github/                                     flujos de CI y de publicación, plantillas de issues y pull requests
├── build.sh build.bat                           compila y ejecuta las pruebas en el ordenador con un panel en vivo
├── eh3d.project.json                            manifiesto: versión, rol, madurez y comando de compilación
├── CHANGELOG.md                                 qué cambió en cada versión
├── SECURITY.md SUPPORT.md                       cómo informar de una vulnerabilidad y dónde preguntar
└── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   cómo contribuir, el código de conducta y la licencia
```

</details>

## 🗺️ Hoja de ruta y límites conocidos

**Límites conocidos hoy**

* Nada se ha ejecutado en una placa: no se ha grabado ninguna imagen, no se ha hecho la puesta en marcha en un nodo real, ningún programa ha hablado con uno y nada se ha conectado a una máquina.
* La placa portadora con el convertidor RS-232, las entradas de LED y el variador del husillo es un diseño sobre el papel (`docs/BOARD.md`).
* El nodo no tiene entrada de parada de emergencia (la placa portadora tiene sitio para un pulsador) y ve los finales de carrera solo a través de los LED.
* La actualización desde GitHub está construida y nunca se ha ejecutado contra el GitHub real en una placa; las versiones no están firmadas, así que confía en el hash que publican. El panel se ha visto en un navegador contra un nodo de pega, no servido desde una placa.
* El final del trabajo retenido no se ha ejecutado en una placa; sus reglas se prueban en un ordenador.
* Solo se pretenden las máquinas Bungard CCD MTC y CCD/2 MTC; los modelos ATC no están cubiertos.

**Siguiente**

* Grabar una imagen en cada placa, hacer la primera puesta en marcha y comprobar el enlace con la biblioteca de Rust.
* Dibujar y construir la placa portadora y conectar un variador de husillo y una máquina.
* Añadir la entrada de pulsador para una parada física.
* Firmar las actualizaciones y descargarlas de GitHub.

## 🌍 Idiomas

El README y los textos existen en inglés, español, francés, italiano, alemán, chino, japonés y portugués de Portugal.

## 🔗 Proyectos relacionados

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): el programa de taladrado; maneja la máquina a través de este nodo con `connection = node`
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): el programa de fresado; comparte la biblioteca del protocolo y el fichero de máquina
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): las partes comunes de la familia EH3D; allí vive la base del firmware del nodo
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): el centro de la familia EH3D: ejecuta los trabajos de los programas y de los demás clientes en la máquina a través de este nodo

## 📚 Documentación

* [El enlace de control](docs/PROTOCOL.md): tramas, inicio de sesión, armado, cola, husillo, LED y las reglas que detienen la máquina.
* [El firmware](docs/NODE_FIRMWARE.md): puesta en marcha, ajustes, panel, actualización con vuelta atrás, memoria y autoprotección.
* [Cableado](docs/WIRING.md): el puerto RS-232, los LED y el variador del husillo.
* [La placa portadora](docs/BOARD.md): el diseño a partir del cual dibujar el esquema.
* [Registro de cambios](CHANGELOG.md)

## 🤝 Contribuir y comunidad

* Antes de una pull request ejecuta las pruebas en el ordenador (`./build.sh` o `build.bat`) y `python -I tools/ci_validate.py`: todo debe pasar. Consulta [CONTRIBUTING.md](CONTRIBUTING.md) y el [código de conducta](CODE_OF_CONDUCT.md).
* El README existe en ocho idiomas: un cambio en el texto se hace en los ocho archivos, y las dos imágenes se dibujan a partir de `docs/architecture.json` y `docs/structure.json` con `tools/make_architecture.py` y `tools/make_structure.py`.
* Nada que envíe bytes a una unidad de control real cambia sin una nota de lo observado en el hardware (o con un analizador lógico) y una prueba con el simulador o el nodo de pega.
* Informa de una vulnerabilidad en privado: [SECURITY.md](SECURITY.md). Preguntas, ideas e informes: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCIA

Software: GPL-3.0-or-later. Véase [LICENSE](LICENSE). Los diseños de hardware de la suite usan CERN OHL-S.
