# Changelog

## v1.0-nx

Primeira versão para Nintendo Switch.

- **Build NRO nativo** (ARM64, 64-bit, independente de posição) com OpenGL 3.3 via Mesa / nouveau.
- **Mira por giroscópio** em modo portátil, Pro Controller e Joy-Con: *desligado*, *só mirando* ou
  *mira e câmera*, com velocidade de 10 % a 400 % e inversão dos eixos X e Y.
- **Menu rápido L3 + R3**, com duas abas (L / R):
  - **Settings:** câmera e mira, giroscópio, imagem, som e jogo;
  - **Cheats:** vida infinita, curar, munição infinita, dar item com quantidade, e o inventário atual.
- **− e + juntos** abre todas as opções; com **ZR** também, o menu completo de cheats.
- Os botões A / B seguem o padrão do Switch nos menus.
- Ícone em alta resolução.
- Arquivos do jogo em `sdmc:/switch/recvx/`: a ISO, as opções, os saves, os mods e o log.
- `env.txt` para variáveis de depuração, e medição de desempenho com `RECVX_GPU_TIME=1`.

O histórico do port de PC em que este se baseia está em [docs/CHANGELOG_PC.md](docs/CHANGELOG_PC.md).
