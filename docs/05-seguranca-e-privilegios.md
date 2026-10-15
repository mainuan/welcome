# Segurança e privilégios

## Regras aplicadas

- IDs Flatpak aceitos são uma allowlist fechada em `PackageService`.
- Layouts aceitos também são uma allowlist fechada.
- Cores aceitas são hexadecimais validadas por `QColor`.
- URLs aceitas têm apenas esquemas `http`, `https` ou `tg`; a aplicação não abre `file:` nem executa URL como comando.
- `QProcess::start(program, arguments)` recebe argumentos separados.
- Não há `system`, `popen`, `bash -c`, `sh -c`, `sudo` ou terminal privilegiado no produto final.
- Não há telemetria, analytics, coleta de hardware, token ou credencial.
- A chave Pix não é embutida no binário; a leitura fica limitada aos arquivos de configuração definidos.

## Privilégios

Instalação de aplicativos usa Flatpak com escopo `--user`, evitando root por padrão. Remoção de uma instalação de sistema é delegada ao próprio Flatpak, que pode solicitar autenticação via polkit conforme o sistema; nenhum helper recebe parâmetros arbitrários.

Antivírus e codecs não são instalados silenciosamente. A aplicação abre o Discover para o usuário concluir a operação, quando disponível. Firewall e drivers abrem os módulos oficiais da distribuição e tratam a ausência como erro visível.

Não foi criado um helper root próprio porque o escopo atual não define uma política de pacotes Mainuan. Se uma versão futura precisar instalar APT automaticamente, deverá adotar PackageKit/KAuth ou um helper polkit com métodos e pacotes allowlisted, fora do frontend.
