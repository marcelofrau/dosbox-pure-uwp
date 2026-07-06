# Hello World Gráfico para DOS — Guia de Build

## Estrutura

```
docs/tools/inputtest-better/
├── hello.c                  ← código fonte C
├── Makefile                 ← automatiza build
├── dist/
│   ├── hello.obj            ← objeto compilado
│   ├── hello.exe            ← DOS 16-bit EXE (31 KB)
│   └── inputtest-better.dosz  ← zip do .exe p/ DOSBox Pure
└── docs/
    └── build-guide.md       ← este arquivo
```

## hello.c

Usa Open Watcom Graphics Library (`graph.lib` + `_graph.h`).

- Modo `_VRES16COLOR` → 640×480, 16 cores (VGA)
- Desenha barras coloridas (cores 0-15)
- Caixa branca com borda azul
- Texto "Hello, DOS!" em amarelo no centro
- Aguarda tecla (`_getch()`), restaura modo texto (`_DEFAULTMODE`)

## Makefile

GNU Make (`make.exe` via scoop shim). Três targets:

| Comando | O que faz |
|---------|-----------|
| `make all`   | Compila → link → zip |
| `make copy`  | Copia `.dosz` para `E:\PC\DOSBoxPure\` |
| `make clean` | Remove `dist/` |

### Fluxo interno

1. **Compilação**: `wcc.exe -ms -I=$(WATCOM)\h hello.c` → `hello.obj`
2. **Link**: `wlink.exe system dos file ... library clibs.lib library graph.lib libpath $(WATCOM)\lib286\dos`
3. **Zip**: `Compress-Archive` (PowerShell)
4. **Copy**: `copy /Y` para `E:\PC\DOSBoxPure\`

### Dependências

| Ferramenta | Localização |
|------------|-------------|
| Open Watcom C16 | `C:\Apps\OW\binnt\wcc.exe` |
| Open Watcom Linker | `C:\Apps\OW\binnt\wlink.exe` |
| Headers | `C:\Apps\OW\h\` |
| Libs 16-bit | `C:\Apps\OW\lib286\dos\` |
| Libs gráficas | `C:\Apps\OW\lib286\dos\graph.lib` |
| GNU Make | scoop shim → `make` |

## Dificuldades encontradas

### 1. wcl.exe não encontra wcc.exe

`wcl.exe` (driver compilador+linker) tenta chamar `wcc.exe` mas falha com
`Unable to find "wcc.exe"`. O motivo exato não ficou claro — pode ser um bug
da versão 2.0beta1 ao construir caminhos internamente.

**Solução**: chamar `wcc.exe` e `wlink.exe` separadamente, eliminando o driver.

### 2. Headers não encontrados

`wcc.exe` (compilador 16-bit) não usa `%WATCOM%` para achar includes
automaticamente (diferente do `wcc386.exe` 32-bit).

**Solução**: passar `-I=C:\Apps\OW\h` explicitamente no CFLAGS.

### 3. Linker não resolve %WATCOM% vindo do cmd.exe

`wlink.exe` lê `wlink.lnk` que faz `@%watcom%\binw\wlsystem.lnk` e
`%WATCOM%/lib286/dos` (via libpath). Quando `WATCOM` é setado via
`set WATCOM=...` do cmd.exe, o linker falha com:

```
Error! E2093: environment watcom: cannot open C:\Apps\OW
```

Curiosamente, funciona perfeitamente quando `$env:WATCOM` é setado via
PowerShell. Motivo: misterioso — talvez diferença na forma como cmd.exe vs
PowerShell passa o environment block via CreateProcess, ou um bug no
`wlinkd.dll` ao parsear o `wlink.lnk`.

**Solução**: usar PowerShell para os recipes do Makefile, que garante que
`$env:WATCOM` seja herdado corretamente pelo processo filho.

```makefile
$(HELLO_EXE):
powershell -NoProfile -Command "$$env:WATCOM='C:\Apps\OW'; ... & 'wlink.exe' ..."
```

### 4. Compress-Archive no Makefile

PowerShell `Compress-Archive` precisa de `-Force` para sobrescrever .dosz
existente. Sem `-Force`, falha na segunda execução do `make all`.

### 5. cmd.exe e variáveis de ambiente em recipes

GNU Make no Windows executa cada linha do recipe num cmd.exe separado.
`set VAR=valor` numa linha não persiste pra linha seguinte. Necessário
encadear com `&&` ou usar PowerShell.

## Como buildar

```powershell
cd docs\tools\inputtest-better
make all        # gera dist\inputtest-better.dosz
make copy       # copia pra E:\PC\DOSBoxPure\
```

Para testar: abra `inputtest-better.dosz` no DOSBox Pure, execute
`hello.exe`, veja a tela VGA 640×480 com "Hello, DOS!".
