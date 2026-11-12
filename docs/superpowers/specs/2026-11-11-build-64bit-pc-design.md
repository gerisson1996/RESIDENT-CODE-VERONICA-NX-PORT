# Fase 1 — build 64-bit no PC

Data: 2026-11-11 · Branch: `nx/64bit` (repositório e submódulo `decomp`) · Base: tag `1.1.0`

## Objetivo

Fazer o Project-Veronica compilar e rodar como executável x86-64 no Windows, com
comportamento idêntico ao build 32-bit atual. É o pré-requisito do port para Nintendo
Switch (fase 2), que é ARM 64-bit e não permite escolher o endereço de carga.

Uso pessoal, com a ISO do próprio usuário (base `SLUS_201.84`, NTSC, com dublagem BR).

## Fora de escopo

- Switch, devkitA64, ARM, SDL2/SDL3 no Switch, OpenGL no Switch (fase 2).
- Co-op, HDR, texturas HD, overlay: só precisam continuar compilando.
- Build Linux e MSVC/Clang.

## Situação atual

- O `CMakeLists.txt` da raiz aborta se `CMAKE_SIZEOF_VOID_P` não for 4.
- O executável é linkado em `0x30000000` e o scratchpad é mapeado em `0x70000000`
  (`pc/src/ps2/memory.c`), para que os aliases do PS2 (`addr | 0x20000000`,
  `addr | 0x30000000`) sejam identidades e os endereços caibam nos campos de 32 bits
  dos pacotes DMA.
- Dados do disco com ponteiros reescritos no lugar: só em `binfunc.c` (22 atribuições,
  cerca de 16 campos das estruturas Ninja: `child`, `sibling`, `model`, `vlist`, `plist`,
  `textures`, `filename`, `meshsets`, `meshes`, `mats`, `points`, `normals`, `vertcolor`,
  `vertuv`, `attrs`, `p`), mais `effsub5.c` (`pPointTop`, `pSnowVector`) e `message.c`
  (`mes_dp`).
- Salas (`room.c`) usam `&sys->rdtp[offset]`, que já é independente de largura.
- Cerca de 220 casts ponteiro → inteiro no código do jogo, incluindo `| 0x30000000`,
  `| 0x80000000` em `sceDmaSend` e alinhamentos do tipo `(unsigned int)(p + 63) & ~0x3F`.

## Etapas

Cada etapa termina com o build 32-bit ainda idêntico e os critérios de validação passando.

### Etapa 0 — referência 32-bit

1. Instalar o toolchain `mingw-w64-i686` (GCC, CMake, Ninja) no `C:\msys64` via `pacman`.
2. Compilar a tag `1.1.0` sem alterações com `cmake/mingw-i686.cmake` (ou um arquivo de
   toolchain novo para MSYS2 nativo, se o existente só servir para compilação cruzada).
3. Confirmar que roda com a ISO do usuário: menu, novo jogo, primeira sala.
4. Rodar `pc/tools/layout_check.sh` com o mesmo binário duas vezes, para confirmar que a
   ferramenta funciona neste PC (bash do MSYS2, `SDL_VIDEO_DRIVER=offscreen`).
5. Guardar o binário como `ref32`.

Se a ISO dublada não rodar no build 32-bit sem alterações, parar e reportar: é um
problema anterior ao port.

### Etapa 1a — largura (x86-64 linkado em `0x30000000`)

Os endereços continuam abaixo de 4 GB; só o tamanho do ponteiro muda.

**Tipo slot.** Em `pc/include/pc_ptr.h`, incluído por `pc_prelude.h`:

- `ps2ptr_t`: inteiro de 32 bits sem sinal que guarda um endereço PS2.
- `P32(tipo, slot)`: slot → ponteiro do tipo dado.
- `S32(ptr)`: ponteiro → slot.
- `PS2_ADDR(ptr)` / `PS2_HOST(addr)`: para aritmética de endereço e pacotes.
- No build 32-bit todas são casts simples. No build 64-bit da etapa 1a são casts com
  verificação (em debug) de que o endereço cabe em 32 bits.

**Estruturas do disco.** Os campos ponteiro das estruturas listadas em "Situação atual"
passam a ser `ps2ptr_t` dentro de `#ifdef TARGET_PC`, nos dois builds de PC (32 e 64-bit);
fora de `TARGET_PC` o tipo original é mantido. Cada uso passa por `P32`/`S32`. No 32-bit
isso não muda tamanho nem código gerado, e o critério 1 da validação confirma. A lista completa de estruturas é
fechada na primeira tarefa do plano, lendo `binfunc.c`, `ninjastr.h`, `njplus.h` e
`types.h`; uma estrutura entra na lista se algum arquivo do disco é interpretado
diretamente como ela.

**Estruturas de tempo de execução.** Mantêm ponteiros nativos (8 bytes). Sem mudança no
código, exceto onde houver tamanho numérico fixo.

**Verificação de layout.** `pc/tools/gen_layout_asserts.py` compila um arquivo de sondagem
no build 32-bit, extrai `sizeof` e `offsetof` de cada estrutura do disco e gera
`pc/include/layout_asserts.h` com `_Static_assert`. O build 64-bit inclui esse arquivo;
se um layout divergir, não compila.

**Avisos como erro** no alvo `recvx_game` e `recvx_platform` do build 64-bit:
`-Werror=pointer-to-int-cast`, `-Werror=int-to-pointer-cast`, `-Werror=int-conversion`,
`-Werror=incompatible-pointer-types`, `-Werror=implicit-function-declaration`.
Os casts do jogo são trocados por `PS2_ADDR`/`PS2_HOST` ou por `intptr_t`, conforme o
valor seja um endereço PS2 (vai para pacote, sofre `|`/`&` de alias) ou só aritmética local.

**Build.** A verificação de 4 bytes na raiz vira "4, ou 8 com `RECVX_64=ON`". Novo
`cmake/mingw-x86_64.cmake`. Mantém `--image-base,0x30000000`, ASLR desligado e o
scratchpad em `0x70000000`.

### Etapa 1b — posição (carregável em qualquer endereço)

1. `ps2_base`: endereço host que corresponde ao endereço PS2 `0x30000000` (início da
   imagem). `P32`/`S32`/`PS2_ADDR`/`PS2_HOST` somam ou subtraem essa base.
2. O scratchpad deixa de exigir `0x70000000`: vira um buffer comum, e `PS2_HOST` traduz a
   faixa `0x7000xxxx` para ele (`memory.c` já faz isso na direção PS2 → host).
3. Os aliases `| 0x20000000`, `| 0x30000000`, `| 0x80000000` e `& 0xFFFFFF` passam a operar
   sobre o endereço PS2, nunca sobre o ponteiro host.
4. Memória alocada pelo host que entra em pacotes DMA sai de uma arena dentro da imagem
   (`.bss`), para ter endereço PS2 válido.
5. Build com ASLR ligado e sem `--image-base`; a verificação de janela em `memory.c` passa
   a checar que a imagem inteira cabe em 256 MB a partir de `ps2_base`.

## Riscos conhecidos

| Risco | Como é tratado |
|---|---|
| Áreas de trabalho de inimigos acessadas por offset numérico (`EXP0_I(0x38)` e similares) podem guardar ponteiros | Tarefa de auditoria na 1a: listar todos os acessos por offset com tipo ponteiro; cada caso vira slot ou campo nomeado |
| Saves podem gravar estruturas com ponteiros | Auditoria de `saves.c`/`savestore.c`/`save_glue.c` na 1a; se houver, o campo é gravado como slot para manter compatibilidade com saves 32-bit |
| O build nativo no MSYS2 não é testado pelo autor | Etapa 0 isola esse problema antes de qualquer mudança de 64-bit |
| O teste automático cobre uma sala | Sessão manual descrita em "Validação"; erros em outras salas são corrigidos conforme aparecerem |
| Leituras de memória não inicializada que dependem do layout (já documentadas em "Determinism") | Se os quadros divergirem entre 32 e 64-bit, investigar primeiro como bug de layout, usando `RECVX_LAYOUT_PAD` |
| Código da plataforma que guarda `void*` em `uint32_t` (9 arquivos de `pc/src` usam `uintptr_t`) | Os avisos como erro apontam cada ponto; todos passam por `PS2_ADDR`/`PS2_HOST` |

## Validação

Critérios para dar cada etapa (1a e 1b) como concluída:

1. O build 32-bit do branch gera quadros idênticos aos de `ref32` (`layout_check.sh`).
2. O build 64-bit compila sem os avisos listados e com `layout_asserts.h`.
3. Os testes unitários (`ctest`) passam em 32 e 64-bit.
4. `layout_check.sh ref32 build64` não acusa diferença, em vanilla e enhanced.
5. Sessão manual no build 64-bit: novo jogo até o primeiro save, carregar o save, abrir
   inventário e mapa, um combate, uma porta, um vídeo.
6. Só na 1b: o critério 4 passa em duas execuções com endereços de carga diferentes.

Se o critério 4 falhar por diferença de ponto flutuante entre x87/SSE de 32-bit e SSE de
64-bit (os dois usam `-mfpmath=sse`, então não é esperado), o critério passa a ser
diferença zero entre dois builds 64-bit com layouts diferentes, mais comparação visual
com `compare_frames.py` contra `ref32`.

## Organização do trabalho

- Mudanças na decompilação vão no submódulo `decomp`, branch `nx/64bit`, sempre dentro de
  `#ifdef TARGET_PC` ou atrás das macros de `pc_ptr.h`.
- Mudanças de plataforma vão em `pc/`.
- Nada derivado da ISO (quadros, dumps, saves) entra no repositório.
