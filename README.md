# 🍊 tinyOrangeOS

[<kbd> 🚀 Testar no QEMU </kbd>](#-como-testar-no-qemu) 
[<kbd> 💿 Testar em PC Real </kbd>](#-como-testar-num-pc-real) 
[<kbd> 🛠️ Como Compilar </kbd>](#%EF%B8%8F-como-compilar) 
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
git clone [https://github.com/Nic24nix/tinyOrangeOS.git](https://github.com/Nic24nix/tinyOrangeOS.git)
cd tinyOrangeOS

# Compilar o código e gerar os binários
make
```

---

## 📁 Estrutura do Repositório

| Ficheiro/Pasta | Descrição |
| :--- | :--- |
| `kernel/` | Código-fonte principal do kernel em C |
| `boot/` | Código de arranque em Assembly (NASM) |
| `Makefile` | Script de automatização da compilação |
| `linker.ld` | Script do linker para organizar as secções de memória |

---

> **Desenvolvido por [Nic24nix](https://github.com/Nic24nix)** 🍊
