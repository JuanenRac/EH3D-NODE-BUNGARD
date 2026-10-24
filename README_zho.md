<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | 🇨🇳 <b>中文</b> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### 一个装在 Bungard CCD 机器箱内的 ESP32-S3 节点，接在其控制单元的 RS-232 口上，让 EH3D 程序通过以太网或 Wi-Fi 驱动机器，机器的指示灯和主轴驱动器也在同一块板上

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**诚实性核对 - 今天真正能运行的内容：** 面向 ESP32-S3（ESP-IDF 5.5）的 C++17 固件，把 Bungard CCD 机器控制单元的 RS-232 口变成网络口：程序用访问令牌通过 TCP 连接，解锁机器并发送一个动作的字节，节点把它们排队并按线路自身的节拍送出。它还读取机器的指示灯（HOME X、Y、Z，ENABLE，TOOL），通过 0-10 V 速度信号和运行/方向线控制主轴，并提供一个网页面板（八种语言），用于初始设置、用户、网络、更新和机器自身的设置。链路规则和设置在电脑上测试，固件在两种板子上都能无警告编译。**它还没有烧录或在板子上运行过**，而把 RS-232 转换器、指示灯输入和主轴驱动器合在一起的载板只是纸面设计。

---

> **安全（safety）与信息安全现状：** 这不是安全系统。接线错误的节点或设置错误的主轴驱动器可能让机器动起来或让刀具旋转。没有访问令牌节点拒绝被控制，只有在程序解锁后才接收给机器的字节，并在会话沉默或连接断开时停止（清空队列，关闭步进驱动器和主轴；除非开启了让它把已持有的作业做完的可选设置）。它只能通过指示灯看到机器的限位开关，也还没有自己的紧急停止（载板留有按钮输入的位置）。主轴驱动器接市电：请按厂家手册接线和设置。面板从不移动机器，并且在程序让机器处于解锁状态时拒绝保存设置。本项目是独立的：它不是 Bungard Elektronik 制造、认可或支持的，产品名称只是用来说明它面向哪些机器和驱动器。

## 📋 概览

| | |
|---|---|
| 版本 | `0.0.2` |
| 成熟度 | 脚手架 |
| 测试 | 9 个主机测试套件（约 2200 项检查） |
| 技术栈 | C++17、ESP-IDF 5.5、ESP32-S3 |
| 真实硬件 | 否：两种固件镜像都能编译，但还没有在板子上运行过任何东西 |

## 🎯 简介

* **控制链路**（TCP，默认端口 5800）：带 CRC 的帧协议、用访问令牌登录、解锁、给控制单元的字节队列，以及会话沉默或断开时的停止。程序发送已按间隔排好的动作字节，节点的 UART 把它们精确地送到线路上，这是 USB 串口适配器在高速时做不到的。
* **连接控制单元的串口线**：通过 MAX3232 的 38400 波特 8N1，以及握手用的 DTR、DCD 和 RI 线。引脚以及收发器是否反相，都是设置项。
* **指示灯**：HOME X、HOME Y、HOME Z、ENABLE 和 TOOL 通过缓冲晶体管读取，显示在状态和面板上，并且一个请求可以让节点在某个轴到达原点的瞬间丢弃搜索的剩余部分，因此程序不必再在数据流中间读取原点线。
* **主轴**：给 0-10 V 转换器的速度 PWM（默认满信号 24000 rpm，适用于 Huanyang HY02D223B 之类的变频器）和正转/反转运行线，旋转时或刚停止后不会换向，并且机器一停主轴就停。
* **面板**（HTTP 和 HTTPS，八种语言）：用 USB 控制台显示的代码做首次设置、用户、以太网或 Wi-Fi、控制链路、RS-232 引脚、指示灯和主轴、日志，以及上传到两个固件槽之一并可回滚的更新。镜像里没有编译任何机密。
* **自我保护**：监视任务和内存的守卫、内存不足时面板入口的闸门，以及节点可访问后对新镜像的确认。
* **板子**：Waveshare ESP32-S3-ETH（W5500 以太网）和 ESP32-S3-WROOM-1 N16R8（Wi-Fi），各有一个镜像。载板（100 x 100 mm）的设计在 `docs/BOARD.md`。
* **机器**：使用 DOS 软件 4.6 串口控制单元的 Bungard CCD MTC 和 CCD/2 MTC。两种 ATC 型号不在范围内；XL 型号及之后的机器带有自己的现代控制单元。
* **自主完成**（默认关闭，是一项设置）：程序或服务器消失时，节点把队列中已有的字节（包括其间排入的主轴命令）执行到底，然后关闭主轴和驱动器。队列放在外部 RAM 中（最多 4 MB，数分钟的运动），时间上限、Abort 和面板上的停止按钮都可以结束它。**计划中，尚未实现**：用于物理停止的按钮输入。

## 🧩 在 EH3D 中的角色

EH3D-NODE-BUNGARD 是 EH3D 程序与机器之间的最后一环：现在 EH3D-DRILLPRO 和 EH3D-ROUTEPRO 连接它，之后 EH3D-SERVER 会放在它前面。它是 EH3D 家族的第一个节点，其设置、面板和更新机制是为与后续节点共享而做的（公共基础放在 EH3D-COMMON 中）。

## 📂 仓库结构

```text
EH3D-NODE-BUNGARD/
├── core/         链路的帧和会话规则、字节队列和设置：无硬件，在电脑上测试
├── main/         ESP-IDF 固件：UART、控制链路、指示灯、主轴、网络、设置存储、网页服务器
├── panel/        打包进固件的网页面板
├── tests/        主机测试和面板文字检查
├── tools/        build_node.sh、pack_panel.py、check_sdkconfig.py、ci_validate.py、panel_mock.mjs
├── docs/         PROTOCOL、NODE_FIRMWARE、WIRING 和 BOARD
├── images/       横幅
└── eh3d.project.json
```

## 🛠️ 构建与运行

主机测试需要 C++17 编译器和 CMake，固件需要 ESP-IDF 5.5（或 Docker）。

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth`（或 `s3-wifi`）在 ESP-IDF 容器中构建镜像；在 Windows 上，已安装的 ESP-IDF 5.5.5 也一样。镜像 `dist/eh3d-node-bungard-<版本>-<板子>-complete.bin` 只用于一种板子：用 `python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <镜像>` 烧录到地址 0。然后连接节点开启的 Wi-Fi `EH3D-SETUP-xxxxxx`，打开 `http://192.168.4.1/`，输入节点在其 USB 控制台显示的设置代码；访问令牌、网络和引脚都在面板中设置。

## 🔗 相关项目

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): 钻孔程序；用 `connection = node` 通过这个节点驱动机器
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): 铣削程序；共用协议库和机器文件
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): EH3D 家族的公共部分；节点固件的基础放在那里
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): EH3D 家族的中心：通过这个节点在机器上执行程序和其他客户端的作业

## 📚 文档与社区

* [控制链路](docs/PROTOCOL.md): 帧、登录、解锁、队列、主轴、指示灯，以及让机器停止的规则.
* [固件](docs/NODE_FIRMWARE.md): 初始设置、设置项、面板、可回滚的更新、内存和自我保护.
* [接线](docs/WIRING.md): RS-232 口、指示灯和主轴驱动器.
* [载板](docs/BOARD.md): 用来画原理图的设计.
* [更新日志](CHANGELOG.md)
* [参与贡献](CONTRIBUTING.md)
* [安全政策](SECURITY.md)
* [支持](SUPPORT.md)

## 👤 作者

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 许可证

软件：GPL-3.0-or-later。见 [LICENSE](LICENSE)。套件的硬件设计使用 CERN OHL-S。
