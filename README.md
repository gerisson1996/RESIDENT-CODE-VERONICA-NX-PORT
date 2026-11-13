<div align="center">

<img src="pc/switch/icon.jpg" alt="Resident Evil Code: Veronica X NX" width="220">

# Resident Evil Code: Veronica X — NX Port

**Port nativo de *Resident Evil Code: Veronica X* (PS2) para Nintendo Switch**

![Switch](https://img.shields.io/badge/Nintendo%20Switch-homebrew-e60012?style=for-the-badge&logo=nintendoswitch&logoColor=white)
![ARM64](https://img.shields.io/badge/ARM64-nativo-1f6feb?style=for-the-badge)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3-5586a4?style=for-the-badge&logo=opengl&logoColor=white)
![Gyro](https://img.shields.io/badge/mira-girosc%C3%B3pio-8250df?style=for-the-badge)
![License](https://img.shields.io/badge/licen%C3%A7a-MIT-2ea44f?style=for-the-badge)

🇧🇷 **Português** · 🇺🇸 [English](README.en.md) · 🇪🇸 [Español](README.es.md)

[**Download**](../../releases/latest) · [Instalação](#-instalação) · [Controles](#-controles) · [Compilar](#%EF%B8%8F-compilar) · [Créditos](#-créditos)

</div>

> [!IMPORTANT]
> **Este repositório não contém o jogo.** Você precisa da sua própria cópia original de
> *Resident Evil Code: Veronica X* (PS2, NTSC-U, **SLUS-20184**), copiada por você para um arquivo `.iso`.

---

## 📖 Sobre

O código original do jogo, vindo da decompilação [recvx-decomp](https://github.com/AshfordFamily/recvx-decomp),
é **compilado nativamente para o processador ARM64 do Switch**. **Não é um emulador:** só o hardware do PS2 nas
bordas do jogo (GS, VU, DMA, som, disco, memory card e controle) é reproduzido, e as partes pesadas foram
trocadas por caminhos nativos. O jogo lê a sua ISO direto do cartão SD.

O port é baseado no [Project Veronica](https://github.com/camybiohazard/Project-Veronica), o port nativo para PC.

## ✨ Recursos

| | |
|---|---|
| 🎮 **Nativo** | Código do jogo compilado para ARM64, renderização na GPU com OpenGL 3.3 |
| 🖥️ **Imagem melhorada** | Widescreen, resolução interna maior, quadros intermediários para um movimento suave (a lógica do jogo continua nos 30 FPS originais) |
| 🎯 **Mira por giroscópio** | Funciona no modo portátil, no Pro Controller e nos Joy-Con, com velocidade e inversão ajustáveis |
| 🧍 **Terceira pessoa** | Câmera opcional por cima do ombro, com mira livre e mira automática |
| ⚡ **Menu rápido** | **L3 + R3** abre um menu no estilo de console com as opções principais, sem sair do jogo |
| 🧪 **Cheats** | Vida e munição infinitas, curar, dar qualquer item, atravessar paredes, destravar tudo |
| 💾 **Saves modernos** | Cada save é um arquivo próprio no cartão SD, sem limite de quantidade |
| 🧩 **Mods** | Salas e arquivos substitutos, e pacotes de texturas HD do PCSX2 do jeito que vêm |

## 📦 Requisitos

- Nintendo Switch com homebrew (Atmosphère + Homebrew Menu)
- Sua ISO de *Resident Evil Code: Veronica X* — PS2, NTSC-U, **SLUS-20184**
- Uns 4 GB livres no cartão SD

> [!TIP]
> Abra o Homebrew Menu **segurando R enquanto abre um jogo** (modo title takeover), e não pelo Álbum. No modo
> applet a memória é bem menor e o jogo pode não abrir.

## 🚀 Instalação

1. Baixe o `recvx.nro` na página de [Releases](../../releases/latest).
2. Copie para o cartão SD em `sdmc:/switch/recvx/`.
3. Coloque **a sua** ISO na mesma pasta (qualquer nome terminado em `.iso`).
4. Abra **Code: Veronica X** pelo Homebrew Menu.

```
sdmc:/switch/recvx/
├── recvx.nro      o jogo
├── *.iso          a sua ISO
├── recvx.ini      opções (criado no primeiro início)
├── recvx.log      log, para relatar problemas
├── save/          saves
└── mods/          opcional: mods e texturas HD (mods/textures/)
```

## 🎮 Controles

O jogo usa os controles originais. O port adiciona três menus:

| Botões | Abre |
|---|---|
| **L3 + R3** | **Menu rápido**, com as abas **Settings** e **Cheats** |
| **−** e **+** juntos | Todas as opções, página por página |
| **−** e **+** juntos, segurando **ZR** | Menu completo de cheats e ferramentas |

**Dentro do menu rápido**

| Botão | Ação |
|---|---|
| D-pad / analógico esquerdo | Escolher (cima / baixo) e mudar o valor (esquerda / direita) |
| **A** | Mudar ou confirmar |
| **L** / **R** | Trocar entre **Settings** e **Cheats** |
| **B** | Fechar |

O jogo continua rodando com o menu aberto, mas os comandos não chegam ao jogo até você fechar o menu.

## ⚡ Menu rápido

**Aba Settings**

| Seção | Opções |
|---|---|
| Câmera e mira | Terceira pessoa, giroscópio, velocidade do giroscópio, inverter X / Y, sensibilidade da câmera e da mira, mira automática, mira na tela |
| Imagem | Escala, quadros intermediários, proporção da tela, brilho, mostrar FPS |
| Som | Volume da música e dos efeitos |
| Jogo | Pular as portas, carregamento rápido |

**Aba Cheats**

| Opção | Efeito |
|---|---|
| Infinite health | A vida fica sempre cheia |
| Heal now | Enche a vida uma vez |
| Infinite ammo | A arma equipada nunca fica sem munição |
| Item / Count / Add to inventory | Escolha qualquer item e a quantidade, e coloque no inventário |

As mudanças ficam salvas sozinhas.

## 🎯 Mira por giroscópio

Com a terceira pessoa ligada, o sensor de movimento do controle mira como um mouse:

- **Gyro aiming:** *Off*, *While aiming* (padrão) ou *Aim and camera*
- **Gyro speed:** de 10 % a 400 %
- **Gyro invert X / Y**

Funciona no modo portátil, no Pro Controller e com um par de Joy-Con (quem mira é o Joy-Con direito).

## 🛠️ Compilar

```sh
git clone --recursive https://github.com/gerisson1996/RESIDENT-CODE-VERONICA-NX-PORT.git
cd RESIDENT-CODE-VERONICA-NX-PORT
bash scripts/build-switch.sh -- recvx_nro
```

Precisa do devkitPro (devkitA64, libnx, switch-sdl2, switch-mesa). Detalhes, onde fica o código do Switch e as
opções de depuração: [docs/BUILDING.md](docs/BUILDING.md).

## 📚 Documentação

| Documento | Conteúdo |
|---|---|
| [CHANGELOG.md](CHANGELOG.md) | O que mudou em cada versão do port |
| [docs/BUILDING.md](docs/BUILDING.md) | Compilar para o Switch e depurar |
| [docs/USER_GUIDE.md](docs/USER_GUIDE.md) | Todas as opções, cheats, saves e texturas HD |
| [docs/THIRDPERSON.md](docs/THIRDPERSON.md) | Modo terceira pessoa |
| [docs/modding/](docs/modding/README.md) | Formato das salas e como criar salas novas |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Como o port funciona por dentro |

## 🙏 Créditos

- **[Project Veronica](https://github.com/camybiohazard/Project-Veronica)** — camybiohazard: o port nativo para PC
  em que este port se baseia
- **[recvx-decomp](https://github.com/AshfordFamily/recvx-decomp)** — AshfordFamily e colaboradores: a
  decompilação (MIT)
- **[devkitPro](https://devkitpro.org/)** — devkitA64, libnx e os ports de SDL2 / Mesa para o Switch
- Bibliotecas: [SDL](https://github.com/libsdl-org/SDL), [Dear ImGui](https://github.com/ocornut/imgui),
  [miniupnpc / libnatpmp](https://github.com/miniupnp), [Monocypher](https://github.com/LoupVaillant/Monocypher)
  — detalhes em [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

## ⚖️ Aviso legal

Projeto de fã, não oficial, sem ligação com a Capcom, a Nintendo, a Sony Interactive Entertainment, a Sega ou a
CRI Middleware, e sem o apoio delas. *Resident Evil* e *Code: Veronica* são marcas da Capcom, Nintendo Switch é
marca da Nintendo e PlayStation é marca da Sony Interactive Entertainment. **Nenhum arquivo do jogo é
distribuído:** use a sua própria cópia original. Não compartilhe ISOs nem arquivos gerados a partir delas.

O código deste repositório está sob a [licença MIT](LICENSE). A decompilação em `decomp/` é MIT, dos seus
autores, e os componentes de terceiros mantêm as próprias licenças.
