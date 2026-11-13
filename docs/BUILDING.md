# Compilando para o Nintendo Switch

O port é compilado com o [devkitPro](https://devkitpro.org/) e gera um homebrew `.nro`.

## Requisitos

- devkitPro com **devkitA64** e **libnx**
- Pacotes do Switch: `switch-sdl2`, `switch-mesa`, `switch-libdrm_nouveau`, `switch-glad`
- CMake e Ninja (os do próprio devkitPro)

```sh
dkp-pacman -S switch-dev switch-sdl2 switch-mesa switch-libdrm_nouveau switch-glad
```

## Compilar

Clone com os submódulos (o decomp fica no branch `decomp` deste repositório):

```sh
git clone --recursive https://github.com/gerisson1996/RESIDENT-CODE-VERONICA---NX---PORT.git
cd RESIDENT-CODE-VERONICA---NX---PORT
```

No shell msys2 do devkitPro (Windows) ou num shell com `$DEVKITPRO` definido (Linux / macOS):

```sh
bash scripts/build-switch.sh -- recvx_nro
```

Saída: `build-switch/pc/recvx.nro`.

O build é sempre 64-bit e independente de posição (`RECVX_64=ON`, `RECVX_PIE=ON`), porque o Horizon carrega
o executável no endereço que escolher. Depois de trocar `pc/switch/icon.jpg`, rode `touch pc/CMakeLists.txt`
para o NRO ser refeito com o ícone novo.

## Onde fica o código do Switch

| Caminho | O que é |
|---|---|
| `pc/switch/sdl3compat.c` | A camada SDL3 do jogo sobre o SDL2 do devkitPro |
| `pc/switch/nx_gyro.c` | Giroscópio (SixAxis) para a mira |
| `pc/switch/nx_fatal.c` | Tela de erro com o caminho do log |
| `pc/switch/imgui_sdl2_shim.cpp` | Dear ImGui sobre SDL2 / glad |
| `pc/src/host/overlay.cpp` | Menu rápido L3 + R3 e os menus de opções e cheats |
| `scripts/build-switch.sh` | O script de build |

## Depuração

`sdmc:/switch/recvx/env.txt` define variáveis de ambiente no início, uma `NOME=valor` por linha:

| Variável | Efeito |
|---|---|
| `RECVX_GPU_TIME=1` | Tempos de CPU e GPU por quadro no `recvx.log` |
| `RECVX_SKIP_MOVIES=1` | Pula os vídeos |
| `RECVX_SHOW_OVERLAY=3` | Abre o menu rápido ao iniciar |

O log fica em `sdmc:/switch/recvx/recvx.log`, e um travamento escreve `recvx-crash.log` na mesma pasta.

## PC

O mesmo código ainda compila para Linux e Windows, como no projeto original: veja
[BUILDING_PC.md](BUILDING_PC.md).
