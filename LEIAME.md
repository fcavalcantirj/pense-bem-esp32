# Pense Bem · ESP32

*[Read in English](README.md)* · **[pense-bem-wars.com](https://pense-bem-wars.com)**

Uma recriação funcional do **Pense Bem** (Tectoy, Brasil, 1988) — aquele
brinquedo eletrônico de livro de atividades — numa LilyGO T-Display V1.1. Dois
botões, uma tela colorida, nenhum banco de dados, nenhuma nuvem, e nenhuma
internet necessária para jogar.

Você digita um código de três dígitos, abre o livro correspondente e responde
trinta perguntas de múltipla escolha. O aparelho diz se você acertou.

**Ele não guarda uma única resposta.** Ele *deriva* todas as 14 850 a partir de
duas constantes e umas seis linhas de aritmética — que é justamente o que torna
este projeto interessante.

---

## ⚠ Uma coisa que ele envia

Uma vez por inicialização, a placa faz um POST para `api.pense-bem-wars.com`
contendo **um UUID aleatório que ela mesma gerou, e a versão do firmware. Nada
mais.**

Não envia o endereço MAC. Não envia pontuação. Não envia resposta. Não envia
qual livro você abriu. Não envia onde você está.

A placa avisa isso na tela na primeira vez que é ligada, em português e em
inglês, antes de enviar qualquer coisa.

**Por que isso existe:** antes de publicar o projeto eu defini uma meta —
**50 estrelas no GitHub e 5 placas realmente montadas** — e queria que o segundo
número fosse medido, não chutado. Alguém consegue falsificar essa contagem com um
único `curl`; ela serve para eu decidir se construo a versão multiplayer, e não
como métrica que eu defenderia. O número é *placas que disseram olá*, nunca
*usuários*.

**Para desligar completamente:** coloque `#define PB_PHONE_HOME 0` no
`pense-bem-esp32.ino`. Isso remove a requisição, o UUID, a tela de aviso e toda a
pilha HTTP — 124 KB — e o endereço do servidor some do binário, o que dá para
conferir.

⚠ **Por que esse interruptor importa além de preferência:** o endereço do
servidor é compilado dentro do binário. O domínio está pago até **04/08/2027**
com renovação automática ligada — mas se um dia ele expirar, placas rodando este
firmware passariam a enviar para quem registrasse o domínio depois, e ninguém
além de você pode regravar a sua placa. Esse `#define` é a sua própria defesa,
não um ajuste de gosto.

---

## Rodando

Você compra a placa (uma **LilyGO T-Display V1.1**, cerca de US$ 15), grava o
código e pronto. **Não tem nenhuma fiação** — a placa já vem com a tela, os dois
botões e o carregador de bateria. O único acréscimo opcional é uma cigarra
passiva no GPIO21 para as quatro musiquinhas de fim de rodada.

As instruções completas de instalação estão no [README](README.md#get-it-running)
e no [INSTALL.md](INSTALL.md) — inclusive um prompt pronto para colar no Claude
Code e deixar ele fazer a configuração inteira.

⚠ **Mantenha o nome da pasta `pense-bem-esp32`.** O `arduino-cli` exige que o
nome do arquivo `.ino` seja igual ao da pasta. Um clone com outro nome não
compila — e isso só foi descoberto clonando o repositório publicado.

## Os livros

Os livros impressos originais estão preservados e livres para download no
[Datassette](https://datassette.org/livros/pense-bem). Você precisa de um deles
para jogar: o brinquedo sozinho não tem as perguntas, só as respostas — e nem
isso ele guarda, ele calcula.

## Como o brinquedo funcionava

Ele rodava num **Zilog Z8 com 128 bytes de RAM e 2 KB de ROM**. Não cabia guardar
as perguntas. Não cabia guardar nem as **respostas** — são 14 850. Então ele não
guardava: ele **deriva** cada resposta a partir de `(livro, questão)` com duas
constantes e umas seis linhas de conta. Cada livro impresso, ao longo de anos e
de edições licenciadas do Sonic e da Turma da Mônica, foi escrito para bater com
o que o brinquedo já calculava.

Não estávamos jogando contra um banco de dados. Estávamos jogando contra uma
função pura.

## Créditos e licença

**Eu não descobri a fórmula.** A engenharia reversa é de **Eduardo Habkost**, com
**Leandro Pereira** e **Felipe Sanches**, do Garoa Hacker Clube em São Paulo,
publicada sob Beerware. Sanches também escreveu o driver do MAME para o
relançamento de 2017.

- <https://github.com/lpereira/Pense-Bem>
- <https://github.com/ehabkost/pensebem>

O código deste projeto é MIT. **As constantes em `pensebem.h` não são** — são
trabalho sob Beerware Revisão 42, e o arquivo [NOTICE](NOTICE) precisa viajar
junto com elas, literal, sem paráfrase.

---

**Quer a versão multiplayer?** ⭐ **[Dê uma estrela no repositório](https://github.com/fcavalcantirj/pense-bem-esp32)** —
a estrela é literalmente o voto, e a meta lá em cima é de verdade. Abaixo dela, o
*Pense Bem Wars* não é construído.

☕ [Me paga um café](https://buymeacoffee.com/fcavalcantirj) — nunca obrigatório,
e não há nada à venda aqui. Você compra a sua própria placa e o código é livre.
