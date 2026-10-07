# 🍊 tinyOrangeOS

[<kbd> 🚀 Testar no QEMU </kbd>](#-como-testar-no-qemu)
[<kbd> 💿 Testar em PC Real </kbd>](#-como-testar-num-pc-real)
[<kbd> 🛠 Como Compilar </kbd>](#%EF%B8%8F-como-compilar)
[<kbd> 🌿 tinyLeaf </kbd>](#-tinyleaf)
[<kbd> 📦 Descarregar Releases </kbd>](https://github.com/Nic24nix/tinyOrangeOS/releases)

---

## 📌 Sobre o Projeto

O **tinyOrangeOS** é um sistema operativo leve de 32-bit (x86), desenvolvido do zero em C e Assembly. O projeto foi desenhado especificamente para correr em **dispositivos antigos/fracos** e para ser testado facilmente através de emuladores como o QEMU ou VirtualBox.

Inclui um sistema de ficheiros próprio (**SliceFS**), suporte para múltiplos utilizadores, comandos Unix-like e gestão persistente de dados.

**Versão atual: v0.5.0** 🍊

---

## ✨ Funcionalidades

* **Arquitetura 32-bit (i386):** Compatível com processadores antigos e baixo consumo de recursos.
* **Sistema de Ficheiros SliceFS:** Suporte para diretoria raiz (`/`), navegação de caminhos relativos e absolutos, e persistência de dados.
* **Shell Interativo:** Inclui comandos utilitários como `ls`, `mkdir`, `cd`, `cat`, `echo` e gestão de permissões.
* **Multi-utilizador:** Suporte para utilizadores com diretórias home dedicadas (ex: `/home/nicolas`).
* **tinyLeaf:** Editor de texto integrado diretamente no kernel e desenvolvido especificamente para o tinyOrangeOS.

---

## 🚀 Como Testar no QEMU

Para testar o sistema numa máquina virtual sem necessidade de compilar o código:

1. Acede à aba [Releases](https://github.com/Nic24nix/tinyOrangeOS/releases) e descarrega a ISO (`tinyOrangeOS-v0.5.0-i386.iso`) e o Disco (`tinyOrangeOS-v0.5.0-i386.img`).
2. Abre o terminal na pasta onde descarregaste os ficheiros e executa:

```bash
qemu-system-i386 -cdrom tinyOrangeOS-v0.5.0-i386.iso -drive format=raw,file=tinyOrangeOS-v0.5.0-i386.img
```

> **Nota:** Se estiveres num sistema de 64-bit, também podes usar `qemu-system-x86_64`, mas a versão `i386` garante maior fidelidade com o hardware de 32 bits.

---

## 💿 Como Testar num PC Real

Se quiseres arrancar o tinyOrangeOS num computador antigo através de uma Pen Drive USB:

1. Descarrega o ficheiro `.iso` na aba [Releases](https://github.com/Nic24nix/tinyOrangeOS/releases).
2. Grava a ISO numa Pen Drive utilizando uma ferramenta como o **Rufus** (em modo DD/RAW) ou **BalenaEtcher**.
3. Insere a Pen Drive no PC pretendido, altera a ordem de boot na BIOS/UEFI e arranca o sistema.

---

## 🛠 Como Compilar

Se quiseres modificar o código-fonte e compilar a tua própria versão do kernel:

### Requisitos Prévios

* `gcc` com suporte para compilação 32-bit
* `binutils`
* `make`
* `xorriso`
* GRUB 2 (`grub2-mkrescue` no Fedora)

### Passos para Compilação

```bash
# Clone o repositório
git clone https://github.com/Nic24nix/tinyOrangeOS.git
cd tinyOrangeOS

# Compilar o código e gerar a ISO
make
```

---

## 📁 Estrutura do Repositório

O kernel está dividido por responsabilidade. `kernel/kernel.c` coordena a inicialização e liga os módulos; cada pasta mantém a implementação da sua área.

| Pasta / ficheiro | Responsabilidade |
| :------------- | :------------- |
| `kernel/kernel.c` | Inicialização e integração dos módulos |
| `kernel/core/` | Acesso a portas de hardware |
| `kernel/drivers/` | Console VGA, teclado e disco ATA |
| `kernel/fs/` | SliceFS e gestão de utilizadores |
| `kernel/apps/` | Leaf e neofetch |
| `kernel/shell/` | Shell e encaminhamento de comandos |
| `kernel/lib/` | Funções de memória e strings |
| `kernel/timezone.c` | Relógio e fusos horários |
| `kernel/kernel.h` | Interfaces partilhadas entre módulos |
| `boot/` | Código de arranque em Assembly |
| `Makefile` | Compilação e geração da ISO |
| `linker.ld` | Organização das secções de memória |

## 💻 Comandos da Shell

O tinyOrangeOS inclui uma shell com suporte a comandos de sistema, gestão de utilizadores e manipulação de ficheiros via **SliceFS**:

| Comando    | Descrição                                                | Sintaxe / Exemplo          |
| :--------- | :------------------------------------------------------- | :------------------------- |
| `help`     | Mostra a lista de comandos disponíveis.                  | `help`                     |
| `clear`    | Limpa o ecrã do terminal.                                | `clear`                    |
| `neofetch` | Exibe as informações do sistema e o logótipo em Braille. | `neofetch`                 |
| `uname`    | Mostra a versão do kernel e a arquitetura.               | `uname`                    |
| `pwd`      | Mostra o caminho da diretoria atual.                     | `pwd`                      |
| `ls`       | Lista os ficheiros e pastas da diretoria atual.          | `ls`                       |
| `cd`       | Navega entre diretórias.                                 | `cd <diretoria>`           |
| `mkdir`    | Cria uma nova diretoria.                                 | `mkdir <nome_ou_caminho>`  |
| `touch`    | Cria um ficheiro de texto vazio.                         | `touch <nome_do_ficheiro>` |
| `write`    | Escreve texto dentro de um ficheiro existente.           | `write <ficheiro> <texto>` |
| `cat`      | Exibe o conteúdo de um ficheiro.                         | `cat <ficheiro>`           |
| `rm`       | Remove um ficheiro ou diretoria.                         | `rm <item>`                |
| `user`     | Mostra ou troca o utilizador.                            | `user <nome_utilizador>`   |
| `useradd`  | Regista um novo utilizador.                              | `useradd`                  |
| `leaf`     | Abre o editor tinyLeaf.                                  | `leaf <arquivo.txt>`       |
| `format`   | Formata a partição do SliceFS.                           | `format`                   |
| `shutdown` | Sincroniza o SliceFS e desliga o sistema.                | `shutdown`                 |

---

## 🌿 tinyLeaf

O **tinyLeaf** é o editor de texto oficial do **tinyOrangeOS**, construído de raiz e incorporado diretamente no kernel (*built-in*).

Inspirado em editores clássicos como o **GNU Nano**, o tinyLeaf fornece uma interface completa em modo texto, integrada diretamente com o sistema operativo e com o sistema de ficheiros **SliceFS**.

### ✨ Funcionalidades

* 📝 **Edição de texto:** Criação e edição de ficheiros diretamente através do terminal.
* ⌨ **Atalhos de teclado:** Barra de atalhos na parte inferior da interface.
* 📜 **Scroll automático:** O conteúdo acompanha automaticamente o cursor durante a edição.
* ↔ **Navegação precisa:** Suporte para navegação através das teclas de seta.
* 💾 **Integração com SliceFS:** Leitura e escrita de ficheiros diretamente no sistema de ficheiros do tinyOrangeOS.
* 🧩 **Built-in:** O tinyLeaf está integrado diretamente no kernel.

O tinyLeaf está atualmente em desenvolvimento ativo, com novas funcionalidades e melhorias a serem adicionadas ao longo do desenvolvimento do tinyOrangeOS.

> **tinyLeaf — simples, leve e feito de raiz para o tinyOrangeOS.** 🍊

---


## ORANGE DESKTOP

O Orange Desktop é o mais novo desktop environment do **tinyOrangeOS**

### ✨ Oque ele traz?

* 📝 **Mouse:** com ele tras suporte a drivers para o mouse.
* ⌨ **Terminal:** um terminal grafico
* 📜 **gestor de arquivos:** um gestor de arquivos grafico
* ↔ **Navegação precisa:** Suporte para navegação através das teclas de seta.

### Barra de tarefas

na barra de tarefas existe:

* **Menu iniciar:** No menu iniciar tem: abridor de apps como, o terminal ou o gestor de arquivos, botão de desligar e o terminal only mode
* **Terminal only:** Um modo aonde o desktop é subsituido por um terminal

> **Desenvolvido por [Nic24nix](https://github.com/Nic24nix)** 🍊
