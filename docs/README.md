# Documentação do Mainuan Welcome 1.0

Referências atuais:

- [README principal](../README.md): funcionalidades, instalação e compilação.
- [Arquitetura e manutenção](architecture-and-maintenance.md): serviços,
  segurança, autenticação, backups e recursos externos.
- [Publicação](releasing.md): Actions, tags, checksums e migração Debian.
- [Auditoria da limpeza 1.0](cleanup-1.0.md): inventário e decisões de preservação.

## Histórico de desenvolvimento

Os documentos numerados `00` a `20` e `appearance-and-installer-*.md` registram
auditorias, decisões e execuções anteriores. As versões 2.x são experimentais;
não correspondem à numeração oficial atual. Seus logs e pacotes locais citados
podem ter sido movidos pelo usuário ou removidos como produtos temporários.
Nenhum documento é uma dependência de arquivos pessoais para compilar o Welcome.

As seguintes referências conservam detalhes úteis, sujeitos às correções posteriores:

- [15 — esquemas, Kora e avatar](15-color-schemes-kora-avatar.md): sequência
  assíncrona, permissões, backup de `.face` e limitações do SDDM/AccountsService.
- [16 — conversão inicial das referências](16-layouts-reference-conversion.md):
  histórico de oito opções, posteriormente substituídas pelas sete atuais.
- [17 — correções funcionais](17-layout-functional-fixes.md): widgets, modos de
  comprimento, espessura, compatibilidade do antigo `floating` e rollback.
- [18 — quarta barra](18-classic-fourth-panel.md): etapa anterior ao ajuste de espaçamento.
- [19 — espaçamento final do Clássico](19-classic-manual-spacing.md): geometria,
  âncoras e limitações das medidas simuladas.
- [20 — instalador interno](20-internal-application-installation.md): procedência
  do FreeOffice, hash fixado, transferência protegida e validação privilegiada.

Os relatórios iniciais que descrevem instalação via Discover, cores hexadecimais
ou outros layouts documentam estados antigos. A consolidação atual acima evita
que essas instruções sejam confundidas com o funcionamento da versão 1.0.
