# 🍊 tinyOrangeOS

[<kbd> 🚀 Testar no QEMU </kbd>](#-como-testar-no-qemu)
[<kbd> 💿 Testar em PC Real </kbd>](#-como-testar-num-pc-real)
[<kbd> 🛠️ Como Compilar </kbd>](#%EF%B8%8F-como-compilar)
[<kbd> 🌿 tinyLeaf </kbd>](#-tinyleaf)
[<kbd> 📦 Descarregar Releases </kbd>](https://github.com/Nic24nix/tinyOrangeOS/releases)

---

## 📌 Sobre o Projeto

O **tinyOrangeOS** é um sistema operativo leve de 32-bit (x86), desenvolvido do zero em C e Assembly. O projeto foi desenhado especificamente para correr em **dispositivos antigos/fracos** e para ser testado facilmente através de emuladores como o QEMU ou VirtualBox.

Inclui um sistema de ficheiros próprio (**SliceFS**), suporte para múltiplos utilizadores, comandos Unix-like e gestão persistente de dados.

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

1. Acede à aba [Releases](https://github.com/Nic24nix/tinyOrangeOS/releases) e descarrega a ISO (`tinyOrangeOS-v0.4.0-i386.iso`) e o Disco (`tinyOrangeOS-v0.4.0-i386.img`).
2. Abre o terminal na pasta onde descarregaste os ficheiros e executa:

```bash
qemu-system-i386 -cdrom tinyOrangeOS-v0.4.0-i386.iso -drive format=raw,file=tinyOrangeOS-v0.4.0-i386.img
```

> **Nota:** Se estiveres num sistema de 64-bit, também podes usar `qemu-system-x86_64`, mas a versão `i386` garante maior fidelidade com o hardware de 32 bits.

---

## 💿 Como Testar num PC Real

Se quiseres arrancar o tinyOrangeOS num computador antigo através de uma Pen Drive USB:

1. Descarrega o ficheiro `.iso` na aba [Releases](https://github.com/Nic24nix/tinyOrangeOS/releases).
2. Grava a ISO numa Pen Drive utilizando uma ferramenta como o **Rufus** (em modo DD/RAW) ou **BalenaEtcher**.
3. Insere a Pen Drive no PC pretendido, altera a ordem de boot na BIOS/UEFI e arranca o sistema.

---

## 🛠️ Como Compilar

Se quiseres modificar o código-fonte e compilar a tua própria versão do kernel:

### Requisitos Prévios

* `gcc` (Cross-compiler `i686-elf-gcc` recomendado)
* `nasm` (Assembler)
* `make`
* `xorriso` e `grub-pc-bin` (para gerar a imagem ISO)

### Passos para Compilação

```bash
# Clone o repositório
git clone https://github.com/Nic24nix/tinyOrangeOS.git
cd tinyOrangeOS

# Compilar o código e gerar os binários
make
```

---

## 📁 Estrutura do Repositório

| Ficheiro/Pasta | Descrição                                             |
| :------------- | :---------------------------------------------------- |
| `kernel/`      | Código-fonte principal do kernel em C                 |
| `boot/`        | Código de arranque em Assembly (NASM)                 |
| `Makefile`     | Script de automatização da compilação                 |
| `linker.ld`    | Script do linker para organizar as secções de memória |

---

## 💻 Comandos da Shell

O tinyOrangeOS inclui uma shell com suporte a comandos de sistema, gestão de utilizadores e manipulação de ficheiros via **SliceFS**:

| Comando    | Descrição                                                                  | Sintaxe / Exemplo                                         |
| :--------- | :------------------------------------------------------------------------- | :-------------------------------------------------------- |
| `help`     | Mostra a lista de comandos disponíveis.                                    | `help`                                                    |
| `clear`    | Limpa o ecrã do terminal.                                                  | `clear`                                                   |
| `neofetch` | Exibe as informações do sistema e o logótipo em Braille.                   | `neofetch`                                                |
| `uname`    | Mostra a versão do kernel e a arquitetura.                                 | `uname`                                                   |
| `pwd`      | Mostra o caminho da diretoria atual.                                       | `pwd`                                                     |
| `ls`       | Lista os ficheiros e pastas da diretoria atual.                            | `ls`                                                      |
| `cd`       | Navega entre diretórias (usa `..` para voltar atrás e `/` para a raiz).    | `cd <diretoria>`<br>`cd /home/nicolas`                    |
| `mkdir`    | Cria uma nova diretoria no caminho atual ou num caminho absoluto.          | `mkdir <nome_ou_caminho>`<br>`mkdir pasta`                |
| `touch`    | Cria um ficheiro de texto vazio.                                           | `touch <nome_do_ficheiro>`<br>`touch notas.txt`           |
| `write`    | Escreve texto dentro de um ficheiro existente.                             | `write <ficheiro> <texto>`<br>`write notas.txt Olá Mundo` |
| `cat`      | Exibe o conteúdo de um ficheiro de texto no ecrã.                          | `cat <ficheiro>`<br>`cat notas.txt`                       |
| `rm`       | Remove um ficheiro ou diretoria.                                           | `rm <item>`<br>`rm notas.txt`                             |
| `user`     | Mostra o utilizador com sessão iniciada ou troca de utilizador.            | `user`<br>`user <nome_utilizador>`                        |
| `useradd`  | Regista um novo utilizador e cria a sua pasta em `/home/` *(apenas root)*. | `useradd`                                                 |
| `format`   | Formata a partição do SliceFS e apaga todos os dados do disco `.img`.      | `format`                                                  |
| `shutdown` | Sincroniza o SliceFS com o disco e desliga o sistema em segurança.         | `shutdown`                                                |

---

## 🌿 tinyLeaf

O **tinyLeaf** é o editor de texto oficial do **tinyOrangeOS**, construído de raiz e incorporado diretamente no kernel (*built-in*).

Inspirado em editores clássicos como o **GNU Nano**, o tinyLeaf fornece uma interface completa em modo texto, integrada diretamente com o sistema operativo e com o sistema de ficheiros **SliceFS**.

### ✨ Funcionalidades

* 📝 **Edição de texto:** Criação e edição de ficheiros diretamente através do terminal.
* ⌨️ **Atalhos de teclado:** Barra de atalhos na parte inferior da interface, facilitando a utilização do editor.
* 📜 **Scroll automático:** O conteúdo acompanha automaticamente o cursor durante a edição.
* ↔️ **Navegação precisa:** Suporte para navegação através das teclas de seta.
* 💾 **Integração com SliceFS:** Leitura e escrita de ficheiros diretamente no sistema de ficheiros do tinyOrangeOS.
* 🧩 **Built-in:** O tinyLeaf está integrado diretamente no kernel, sem necessidade de instalar software adicional.

O tinyLeaf está atualmente em desenvolvimento ativo, com novas funcionalidades e melhorias a serem adicionadas ao longo do desenvolvimento do tinyOrangeOS.

> **tinyLeaf — simples, leve e feito de raiz para o tinyOrangeOS.** 🍊

---

> **Desenvolvido por [Nic24nix](https://github.com/Nic24nix)** 🍊
