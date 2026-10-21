# CPU AMD64 básica — Mainuan Welcome 1.0

## Diagnóstico de 21/10/2026

O pacote da [Release v1.0](https://github.com/mainuan/welcome/releases/tag/v1.0)
foi baixado sem alterar os assets e teve seu SHA-256 conferido. O executável
`usr/bin/mainuan-welcome` declara `GNU_PROPERTY_X86_ISA_1_NEEDED = 7`:
`x86-64-baseline, x86-64-v2, x86-64-v3`. Sob QEMU `qemu64` ele termina com
`CPU ISA level is lower than required`. O pacote local anterior, da mesma versão
Debian 1.0, declara somente baseline (máscara 1).

A evidência não depende do nome das bibliotecas. O pacote Ubuntu
`libc6-dev_2.43-2ubuntu2_amd64v3.deb`, obtido do índice oficial
`resolute/main/binary-amd64v3/Packages.xz` e conferido pelo SHA-256 desse índice,
contém `Architecture: amd64` **e** `Architecture-Variant: amd64v3`.
Seu `usr/lib/x86_64-linux-gnu/Scrt1.o` declara máscara 7. O objeto baseline
correspondente declara máscara 1.

Os pacotes oficiais `gcc-15-x86-64-linux-gnu` e `cpp-15-x86-64-linux-gnu`,
versão `15.2.0-16ubuntu1`, também foram extraídos da variante v3. Seu `gcc -v`
informa `--with-arch-64=x86-64-v3`, e `gcc -Q --help=target` confirma o default
`-march=x86-64-v3`. Portanto, a ausência de uma flag explícita no log não
garante codegen baseline. A desmontagem do executável publicado encontrou,
entre outras, `vextracti128`, `vperm2i128`, `vpbroadcastq` e `vfmadd132sd`.
Na desmontagem do executável novo não foram encontrados mnemônicos AVX nem
operandos YMM/ZMM; isso complementa as verificações de notas, flags e execução.

Uma aplicação mínima (`int main(void) { return 0; }`) compilada com
`gcc -march=x86-64 -B<diretório-dos-objetos-v3>` adquire máscara 7 e reproduz o
mesmo erro sob QEMU. Isso demonstra a propagação da exigência pelo objeto de
inicialização da libc na linkedição, mesmo com código-fonte compilado para v1.
Não foi recuperado um mapa de link da execução remota original; portanto não se
atribui cada instrução daquele executável a um objeto específico. A nota do
binário distribuído e o mecanismo de contaminação por startup objects foram
verificados diretamente.
O dump completo da configuração APT do runner original não foi publicado pelo
workflow antigo; o novo job registra esse dado e as origens/versões do toolchain.

Os fontes do Welcome não usavam `-march=native` ou `-march=x86-64-v3`.
O build anterior dependia do ambiente já populado do runner. A seleção de
variantes pode afetar toolchain, objetos de inicialização, Qt e KDE, embora o
pacote final continue declarando a arquitetura Debian `amd64`.

Fontes primárias: [variantes do Ubuntu](https://discourse.ubuntu.com/t/introducing-architecture-variants-amd64v3-now-available-in-ubuntu-25-10/71312),
[índice oficial amd64v3](https://archive.ubuntu.com/ubuntu/dists/resolute/main/binary-amd64v3/Packages.xz).

## Ambiente de distribuição

O job de build continua hospedado em `ubuntu-26.04`, mas compila dentro do
contêiner oficial `ubuntu:26.04`, plataforma `linux/amd64`. Antes do primeiro
`apt-get update`, limpa e desabilita `APT::Architecture-Variants`, limitando os
índices à arquitetura básica. As dependências vêm de `debian/control`.
Compilação, testes e empacotamento executam como usuário sem privilégios.
O manifesto amd64 está fixado por digest no workflow; seu banco dpkg e arquivos
de configuração APT foram auditados sem variantes instaladas/habilitadas.
Atualizar esse digest exige repetir a auditoria; os pacotes de build continuam
recebendo as atualizações dos repositórios Ubuntu 26.04.

`check_cpu.py environment` rejeita variantes já instaladas, independentemente de
`Architecture: amd64`, e audita os defaults do compilador e seus startup objects.
Registra configurações APT, versões/variantes instaladas e `dpkg-buildflags`.
O guard também é chamado por `debian/rules` antes de configurar uma compilação
local: remover a opção APT não torna automaticamente básicos os pacotes existentes.

CMake aplica `-march=x86-64 -mtune=generic` à compilação e à linkedição x86-64,
incluindo LTO. A base limpa e os guards são necessários além dessas flags.
`check_cpu.py flags` confere todos os comandos reais de compilação, recusando
`native`, v2/v3/v4 e extensões globais como AVX2/SSE4/BMI.

## Gates e evidências

As seis suítes obrigatórias continuam sequenciais, paralelas e repetidas pelo
debhelper, com os barramentos D-Bus isolados existentes. Antes do upload:

- Todos os ELF distribuídos são auditados; notas ISA superiores a baseline e
  outras arquiteturas são recusadas. O executável precisa declarar sua ISA.
- A cadeia de bibliotecas resolvida pelo loader para o pacote também é auditada.
- O `.deb` real inicia sob QEMU com `qemu64,-sse3,-cx16,-lahf-lm`.
- Um programa CPUID executado no guest confirma SSE2 e a ausência do conjunto
  v2/v3, incluindo AVX, AVX2, FMA, BMI, POPCNT, SSE4 e LAHF/CX16.
- O teste confirma carregamento de Qt Quick/Controls/Kirigami e inicialização do
  backend software, sem erros de QML/ISA, durante 30 segundos.
- O artefato de Actions inclui os relatórios `cpu-*.json` e `cpu-smoke.log`.

Uma nota ISA ausente em uma biblioteca não prova compatibilidade. Da mesma forma,
a presença de instruções AVX em implementações opcionais selecionadas por CPUID
não implica requisito incondicional. Os guards de ambiente/flags, notas ELF e
execução emulada são complementares. O pacote não incorpora bibliotecas do sistema.

Para validar um ambiente e um pacote já construído:

```bash
python3 .github/scripts/check_cpu.py environment --output build/cpu-environment.json
python3 .github/scripts/check_cpu.py flags build/compile_commands.json
python3 .github/scripts/check_cpu.py package ../mainuan-welcome_1.0_amd64.deb
python3 .github/scripts/check_cpu.py dependencies ../mainuan-welcome_1.0_amd64.deb
python3 .github/scripts/cpu_smoke.py ../mainuan-welcome_1.0_amd64.deb
```

O último comando exige `qemu-user`; ele não instala o Welcome ou aplicativos.
A emulação é de espaço de usuário e compartilha o kernel; não substitui teste
completo da ISO ou validação de Wayland/GPU em hardware antigo.

## Responsabilidade da ISO e distribuição da correção

O alvo do novo pacote é **x86-64 básico (v1/SSE2), sem AVX/AVX2 obrigatório**.
As bibliotecas e os plugins do sistema de destino também precisam ser básicos.
Uma ISO que já contenha libc, Qt, KDE ou outro componente `amd64v3` pode falhar
antes ou independentemente do Welcome. Audite a imagem com `apt-config dump`,
`dpkg-query` (campo `Architecture-Variant`) e as notas ELF das bibliotecas.
Reconstrua a imagem a partir de uma base limpa e valide-a com CPU limitada;
não basta desabilitar variantes APT depois de instalar bibliotecas v3. Não faça
substituição improvisada da libc nem copie bibliotecas para o pacote Welcome.
Outros componentes Mainuan mantêm seus próprios processos de build e auditoria.

A versão permanece 1.0. A tag e os assets de `v1.0` ficam intactos. O mantenedor
pode distribuir o novo artefato de Actions separadamente, identificado por commit
e SHA-256, conforme [a política de recompilações](releasing.md#recompilações-corrigidas-da-versão-10).
O APT não considera a recompilação de mesma versão um upgrade de versão.
